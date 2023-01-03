#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

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


#endif /* userprog/syscall.h */
