#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
void halt (void);
void exit_wrapper(int status);
void exit (int status);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
struct file_descriptor *find_file_by_fd(int fd);
int get_arg(const int *ptr);
void *get_ptr_arg(const void *ptr);
void check_ptr(const void *ptr);
void validate_str(const char *str);
#endif /* userprog/syscall.h */

