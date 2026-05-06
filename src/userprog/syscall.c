#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "threads/synch.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
struct lock files_lock; 

void
syscall_init (void) 
{
  lock_init(&files_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall = get_arg((int *)f->esp);

  switch (syscall)
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    exit_wrapper(get_arg((int *)f->esp + 1));
    break;
  case SYS_EXEC:
    /* code */
    break;
  case SYS_WAIT:
    f->eax = wait(get_arg((int *)f->esp + 1));
    break;
  case SYS_CREATE:
    char *file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    unsigned initial_size = get_arg((int *)f->esp + 2);
    f->eax = create(file, initial_size);
    break;
  case SYS_REMOVE:
    char *file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    f->eax = remove(file);
    break;
  case SYS_OPEN:
    char *file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    f->eax = open(file);
    break;
  case SYS_FILESIZE:
    int fd = get_arg((int *)f->esp + 1);
    f->eax = filesize(fd);
    break;
  case SYS_READ:
    /* code */
    break;
  case SYS_WRITE:
    /* code */
    break;
  case SYS_SEEK:
    /* code */
    break;
  case SYS_TELL:
    /* code */
    break;
  case SYS_CLOSE:
    int fd = get_arg((int *)f->esp + 1);
    close(fd);
    break;
  default:
    // Gamal: should exit.
    break;
  }

  printf ("system call!\n");
  thread_exit ();
}

// Gamal: bey3addee 3ala koll character, yet2akked enno valid, we beyo2aff 3and el null character
// Gamal: in the case the string is not terminated, it must go outside the user space, and thus terminate the program on calling check_ptr()
void validate_str(int *str) {
  while(true) {
    check_ptr(str);
    if (*str == '\0') break;
    str++;
  }

}

void validate_buffer(int *buffer, int size) {
  for (int i = 0; i < size; i++) {
    check_ptr((int *)buffer + i);
  }
}

// Gamal: validates pointer to input, if valid it derefernces it and returns it
// Gamal: Example use: get_arg((int *)fd->esp + i), where 'i' is the agrument number. ya3ny lw 3ayz awl arg, yeb2a 1, lw tany arg yeb2a 2...
static int get_arg(const int *ptr) {
  check_ptr(ptr);
  return *ptr;
}

void *get_ptr_arg(const int *ptr) {
  // Gamal: validate whether pointer to address is valid
  check_ptr(ptr);

  // Gamal: validate whether the address the original pointer points to is valid in
  ptr = *(void **)ptr;
  check_ptr(ptr);

  return ptr;
}


// Gamal: validates whether pointer is valid or not
void check_ptr(const int *ptr) {
  if (ptr != NULL || !is_user_vaddr(ptr) || pagedir_get_page(thread_current()->pagedir, ptr) == NULL) {
    // Gamal: process exits on any memory fault
    exit_wrapper(-1);
  }
}

void halt (void) {
  shutdown_power_off();
}

void exit_wrapper(int status) {
  // Gamal: retrieve thread's child_status struct
  struct child_status *child_status = thread_current()->child_status;
  // Gamal: acquire lock to update child_status struct
  lock_acquire(&child_status->lock);
  // Gamal: check whether the parent process has already exited, if so free the child_status struct
  if (child_status->parent_exited) {
    free(child_status);
  } else {
    // Gamal: update child_status struct with exit status and mark it as exited
    child_status->exit_status = status;
    child_status->is_exited = true;

    // Gamal: release semaphore in case parent is waiting
    sema_up(&child_status->wait_sema);

    // Gamal: remove child_status entry from the parent's list
    list_remove(&child_status->elem);
  }
  lock_release(&child_status->lock);

  // Gamal: release file descriptors
  struct list_elem *e;
  for (e = list_begin(&thread_current()->files); e != list_end(&thread_current()->files); e = list_next(&thread_current()->files)) {
    struct file_descriptor *fd = list_entry(e, struct file_descriptor, elem);
    close(fd->fd);
    free(fd);
  }

  // Gamal: release all semaphores
  for (e = list_begin(&thread_current()->semaphores); e != list_end(&thread_current()->semaphores); e = list_next(&thread_current()->semaphores)) {
    struct semaphore_elem *sema = list_entry(e, struct semaphore_elem, elem);
    sema_up(sema->semaphore);
  }

  thread_exit(); // lock release is not done yet
}

tid_t exec (const char *cmd_line) {
  // To be implemented...
}

int wait (tid_t pid) {
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) {
  lock_acquire(&files_lock);
  bool result = filesys_create(file, initial_size);
  lock_release(&files_lock);
  return result;
}

bool remove (const char *file) {
  lock_acquire(&files_lock);
  bool result = filesys_remove(file);
  lock_release(&files_lock);
  return result;
}

int open (const char *file) {
  lock_acquire(&files_lock);
  struct file *f = filesys_open(file);
  lock_release(&files_lock);

  // if file does not exist, return -1
  if (f == NULL) return -1;

  // create file descriptor struct and add it to the list of open files for the current thread
  struct file_descriptor *fd_struct = malloc(sizeof(struct file_descriptor));
  fd_struct->file = f;
  fd_struct->fd = thread_current()->next_fd;
  thread_current()->next_fd++;
  list_push_back(&thread_current()->files, &fd_struct->elem);

  return fd_struct->fd;
}

int filesize (int fd) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  int size = file_length(fd_struct->file);
  lock_release(&files_lock);

  return size;
}

int read (int fd, void *buffer, unsigned size) {
  // To be implemented...
}

int write (int fd, const void *buffer, unsigned size) {
  // To be implemented...
}

void seek (int fd, unsigned position) {
  // To be implemented...
}

unsigned tell (int fd) {
  // To be implemented...
}

void close (int fd) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  file_close(fd_struct->file);
  lock_release(&files_lock);

  list_remove(&fd_struct->elem);
}

// helper method the find the file by its file descriptor
struct file_descriptor *find_file_by_fd(int fd) {
  struct thread *cur = thread_current();
  struct list_elem *e;

  for (e = list_begin(&cur->files); e != list_end(&cur->files); e = list_next(e)) {
    struct file_descriptor *fd_struct = list_entry(e, struct file_descriptor, elem);
    if (fd_struct->fd == fd) {
      return fd_struct;
    }
  }
  return NULL;
}