#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"
#include "filesys/off_t.h"

typedef int pid_t;

void syscall_init(void);
struct lock filesys_lock;

void exit(int status);
void close(int fd);
void halt(void);
pid_t fork(const char *thread_name);
int exec(const char *file);
int wait(pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned length);
int write(int fd, const void *buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
#endif /* userprog/syscall.h */
