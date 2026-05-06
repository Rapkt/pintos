#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct child_status {
  tid_t tid;
  int exit_status;
  bool is_waited;
  bool is_exited;
  bool parent_exited;
  struct semaphore wait_sema;
  struct lock lock;
  struct list_elem elem;
};

#endif /* userprog/process.h */

