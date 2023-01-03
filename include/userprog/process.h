#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
/* for byte alignment */
#define ALIGNMENT 8
#define FD_LIMIT_LEN 512
#define ADDR_SIZE sizeof(void *)
#define CHARP_SIZE sizeof(char *)

#include "threads/thread.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);
int get_next_fd(struct file **);

#endif /* userprog/process.h */
