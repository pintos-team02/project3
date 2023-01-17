/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "lib/round.h"
#include "threads/mmu.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init(void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer(struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
	struct file *file, off_t offset) {

	void *st_addr = addr;
	struct file *mfile = file_reopen(file);
	struct thread *t_curr = thread_current();
	struct load_info *aux = (struct load_info *)calloc(1, sizeof(struct load_info));
	if (mfile == NULL)
		return NULL;
	
	size_t read_bytes = length > file_length(file) ? file_length(file) : length;
    size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = page_read_bytes == PGSIZE ? 0 : PGSIZE - page_read_bytes;

		struct load_info *mem_init_info = (struct load_info *)calloc(1, sizeof(struct load_info));
		mem_init_info->file = mfile;
		mem_init_info->ofs = offset;
		
		mem_init_info->page_read_bytes = page_read_bytes;
		mem_init_info->page_zero_bytes = page_zero_bytes;

		// void *aux = mem_init_info;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
			writable, lazy_load_segment, (void *)mem_init_info))
			return NULL;
		
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;

		/* 오프셋을 옮겨보자 */
		offset += page_read_bytes;
	}
	
	return st_addr;
}

// /* Do the munmap */
// void
// do_munmap(void *addr) {
// 	printf("do_munmap: called addr is %p\n", addr);
// 	struct supplemental_page_table *spt = &thread_current()->spt;
// 	struct page *page = spt_find_page(spt, addr);
// 	spt_remove_page(spt, page);
// }

void do_munmap (void *addr) {
/* addr부터 연속된 모든 페이지 변경 사항을 업데이트하고 매핑 정보를 지운다.
	가상 페이지가 free되는 것이 아님. present bit을 0으로 만드는 것! */
    while (true) {
		struct page *page = spt_find_page(&thread_current()->spt, addr);
		
		if (page == NULL) {
			return NULL;
		}
		
		struct load_info *container = (struct load_info *)page->uninit.aux;
		
		/* 수정된 페이지(dirty bit == 1)는 파일에 업데이트해놓는다. 이후에 dirty bit을 0으로 만든다. */
		if (pml4_is_dirty(thread_current()->pml4, page->va)){
			file_write_at(container->file, addr, container->page_read_bytes, container->ofs);
			pml4_set_dirty(thread_current()->pml4, page->va, 0);
		}
		
		/* present bit을 0으로 만든다. */
		pml4_clear_page(thread_current()->pml4, page->va);
		addr += PGSIZE;
    }
	
}
