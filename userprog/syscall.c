#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"


void syscall_entry(void);
void syscall_handler(struct intr_frame *);
static void check_address(void *addr);
static bool is_invalid_fd(int fd);
static void intr_frame_cpy(struct intr_frame *f);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init(void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
		((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
		FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler(struct intr_frame *f) {
	/* 포인터 유효성 검증 */
	struct thread *t_curr = thread_current();

	switch (f->R.rax)
	{
	case SYS_HALT:
		halt();
		break;

	case SYS_EXIT:
		exit(f->R.rdi);
		break;

	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;

	case SYS_FORK:
		intr_frame_cpy(f);
		f->R.rax = fork(f->R.rdi);
		break;

	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;

	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;

	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;

	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;

	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;

	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;

	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;

	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;

	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;

	case SYS_CLOSE:
		close(f->R.rdi);
		break;

	default:
		thread_exit();
	}
}

static void
check_address(void *addr) {
	// 1. 포인터 유효성 검증
	//		<유효하지 않은 포인터>
	//		- 널 포인터
	//		- virtual memory와 매핑 안 된 영역
	//		- 커널 가상 메모리 주소 공간을 가리키는 포인터 (=PHYS_BASE 위의 영역)
	// 2. 유저 영역을 벗어난 영역일 경우 프로세스 종료((exit(-1)))
	if (addr == NULL || is_kernel_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL) {
		exit(-1);
	}
}

void
halt(void) {
	power_off();
}

void
exit(int status) {
	struct thread *t_curr = thread_current();
	t_curr->exit_status = status;
	printf("%s: exit(%d)\n", t_curr->name, t_curr->exit_status);
	thread_exit();
}

pid_t
fork(const char *thread_name) {
	check_address(thread_name);
	pid_t pid = process_fork(thread_name, &thread_current()->user_tf);
	return pid;
}

int
exec(const char *cmd_line) {
	check_address(cmd_line);
	char *cmd_line_cpy = palloc_get_page(PAL_ZERO);
	if (!cmd_line_cpy) {
		exit(-1);
	}

	int size = strlen(cmd_line) + 1; // 널 문자가 들어갈 공간
	strlcpy(cmd_line_cpy, cmd_line, size);

	if (process_exec(cmd_line_cpy) == -1) {
		exit(-1);
	}
}

int
wait(pid_t pid) {
	return process_wait(pid);
}

bool
create(const char *file, unsigned initial_size) {
	check_address(file);
	return filesys_create(file, initial_size);
}

bool
remove(const char *file) {
	check_address(file);
	return filesys_remove(file);
}

int
open(const char *file) {
	check_address(file);

	struct thread *t_curr = thread_current();
	struct file *fp = filesys_open(file);
	if (fp)
	{ // FIXME: next_fd 갱신 로직 최적화
		int fd = get_next_fd(t_curr->fdt);
		if (fd == -1)
		{
			file_close(fp);
			return -1;
		}
		t_curr->fdt[fd] = fp;

		t_curr->next_fd = fd; 	// TODO: next_fd가 필요할까?

		return fd;
	}
	return -1;

}

int
filesize(int fd) {
	struct thread *t_curr = thread_current();
	struct file *fp = t_curr->fdt[fd];
	if (fp == NULL) {
		return -1;
	}
	return file_length(fp);
}

int
read(int fd, void *buffer, unsigned size) {
	check_address(buffer);

	if (fd == STDIN_FILENO)
	{
		int i;
		uint8_t key;
		for (i = 0; i < size; i++)
		{
			key = input_getc();
			*(char *)buffer++ = key;
			if (key == '\0')
			{
				i++;
				break;
			}
		}
		return i;
	}
	else if (is_invalid_fd(fd)) {
		return -1;
	}
	else if (fd == STDOUT_FILENO)
	{
		return -1;
	}
	else
	{
		/* 파일 디스크립터에 해당하는 파일을 가져와야한다. */
		struct thread *t_curr = thread_current();
		struct file **fdt = t_curr->fdt;
		// TODO: lock acquire failed
		struct file *curr_file = fdt[fd];
		if (curr_file == NULL)
		{
			return -1;
		}
		lock_acquire(&filesys_lock);
		off_t read_size = file_read(curr_file, buffer, size);
		lock_release(&filesys_lock);
		return read_size;
	}
}

int
write(int fd, const void *buffer, unsigned size) {
	check_address(buffer);


	if (fd == STDOUT_FILENO) {
		putbuf(buffer, size);
		return size;
	}
	else if (fd == STDIN_FILENO) {
		return 0;
	}
	else if (is_invalid_fd(fd)) {
		return 0;
	}
	else
	{
		int read_count;
		struct thread *t_curr = thread_current();
		struct file **fdt = t_curr->fdt;
		struct file *curr_file = fdt[fd];

		if (curr_file == NULL)
		{
			return 0;
		}
		lock_acquire(&filesys_lock);
		read_count = file_write(curr_file, buffer, size);
		lock_release(&filesys_lock);
		return read_count;
	}
}

void
seek(int fd, unsigned position)
{
	struct thread *t_curr = thread_current();
	struct file **fdt = t_curr->fdt;
	struct file *fp = fdt[fd];

	file_seek(fp, position);
}

unsigned
tell(int fd) {
	struct thread *t_curr = thread_current();
	struct file **fdt = t_curr->fdt;
	struct file *fp = fdt[fd];
	return file_tell(fp);
}

void
close(int fd) {// FIXME: next_fd 갱신 로직 최적화
	if (!is_invalid_fd(fd))
	{
		struct thread *t_curr = thread_current();
		struct file *fp = t_curr->fdt[fd];
		if (!fp == NULL)
		{
			file_close(fp);
			t_curr->fdt[fd] = NULL;
		}
	}
}

bool
is_invalid_fd(int fd)
{
	return fd < 0 || fd >= FD_LIMIT_LEN;
}

void
intr_frame_cpy(struct intr_frame *f)
{
	struct thread *t_curr = thread_current();
	memcpy(&t_curr->user_tf, f, sizeof(struct intr_frame));
}