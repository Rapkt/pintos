#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "pagedir.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>

static void syscall_handler(struct intr_frame *);
static void validate_buffer(void *buffer, int size);
struct lock files_lock;

void syscall_init(void) {
  lock_init(&files_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
  int syscall = get_arg((int *)f->esp);
  int fd;
  switch (syscall) {
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
  case SYS_CREATE: {
    char *file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    unsigned initial_size = get_arg((int *)f->esp + 2);
    f->eax = create(file, initial_size);
  } break;
  case SYS_REMOVE: {
    char *file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    f->eax = remove(file);
  } break;
  case SYS_OPEN: {
    char *file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    f->eax = open(file);
  } break;
  case SYS_FILESIZE:
    fd = get_arg((int *)f->esp + 1);
    f->eax = filesize(fd);
    break;
  case SYS_READ:
    fd = get_arg((int *)f->esp + 1);
    char *buffer = get_ptr_arg((int *)f->esp + 2);
    int size = get_arg((int *)f->esp + 3);
    validate_buffer(buffer, size);
    f->eax = read(fd, buffer, size);
    break;
  case SYS_WRITE:
    fd = get_arg((int *)f->esp + 1);
    char *buffer = get_ptr_arg((int *)f->esp + 2);
    int size = get_arg((int *)f->esp + 3);
    validate_buffer(buffer, size);
    f->eax = write(fd, buffer, size);
    break;
  case SYS_SEEK:
    fd = get_arg((int *)f->esp + 1);
    unsigned position = get_arg((int *)f->esp + 2);
    seek(fd, position);
    break;
  case SYS_TELL:
    fd = get_arg((int *)f->esp + 1);
    f->eax = tell(fd);
    break;
  case SYS_CLOSE:
    fd = get_arg((int *)f->esp + 1);
    close(fd);
    break;
  default:
    // Gamal: should exit.
    break;
  }

  // printf("system call!\n");
  thread_exit();
}

// Gamal: bey3addee 3ala koll character, yet2akked enno valid, we beyo2aff 3and
// el null character Gamal: in the case the string is not terminated, it must go
// outside the user space, and thus terminate the program on calling check_ptr()
void validate_str(const char *str) {
  while (true) {
    check_ptr(str);
    if (*str == '\0')
      break;
    str++;
  }
}

static void validate_buffer(void *buffer, int size) {
  for (int i = 0; i < size; i++) {
    check_ptr((char *)buffer + i);
  }
}

// Gamal: validates pointer to input, if valid it derefernces it and returns it
// Gamal: Example use: get_arg((int *)fd->esp + i), where 'i' is the agrument
// number. ya3ny lw 3ayz awl arg, yeb2a 1, lw tany arg yeb2a 2...
int get_arg(const int *ptr) {
  check_ptr(ptr);
  return *ptr;
}
void *get_ptr_arg(const void *ptr) {
  /* ptr is pointer to a stack word that holds an address. */
  check_ptr(ptr);
  void *addr = *(void *const *)ptr;
  check_ptr(addr);
  return addr;
}

// Gamal: validates whether pointer is valid or not
void check_ptr(const void *ptr) {
  if (ptr == NULL || !is_user_vaddr(ptr) ||
      pagedir_get_page(thread_current()->pagedir, ptr) == NULL) {
    exit_wrapper(-1);
  }
}

void halt(void) { shutdown_power_off(); }

void exit_wrapper(int status) {
  /* Update child status as in process_exit */
  struct child_status *child_status = thread_current()->child_status;
  if (child_status != NULL) {
    child_status->exit_status = status;
    child_status->is_exited = true;
    sema_up(&child_status->wait_sema);
  }

  /* Release file descriptors */
  while (!list_empty(&thread_current()->files)) {
    struct list_elem *e = list_pop_front(&thread_current()->files);
    struct file_descriptor *fd_struct =
        list_entry(e, struct file_descriptor, elem);
    file_close(fd_struct->file);
    free(fd_struct);
  }

  // Gamal: remove all terminated entries from the child list
  struct list_elem *e = list_begin(&thread_current()->child_list);
  while (e != list_end(&thread_current()->child_list)) {
    struct child_status *child_status =
        list_entry(e, struct child_status, elem);
    if (child_status->is_exited) {
      struct list_elem *to_be_deleted = e;
      e = list_next(e);
      list_remove(to_be_deleted);
      free(child_status);
    } else {
      e = list_next(e);
    }
  }

  // Gamal: call exit
  exit(status);
}

void exit(int status) { thread_exit(); }

tid_t exec(const char *cmd_line) {
  (void)cmd_line;
  return process_execute(cmd_line);
}

int wait(tid_t pid) { return process_wait(pid); }

bool create(const char *file, unsigned initial_size) {
  lock_acquire(&files_lock);
  bool result = filesys_create(file, initial_size);
  lock_release(&files_lock);
  return result;
}

bool remove(const char *file) {
  lock_acquire(&files_lock);
  bool result = filesys_remove(file);
  lock_release(&files_lock);
  return result;
}

int open(const char *file) {
  lock_acquire(&files_lock);
  struct file *f = filesys_open(file);
  lock_release(&files_lock);

  // if file does not exist, return -1
  if (f == NULL)
    return -1;

  // create file descriptor struct and add it to the list of open files for the
  // current thread
  struct file_descriptor *fd_struct = malloc(sizeof(struct file_descriptor));
  fd_struct->file = f;
  fd_struct->fd = thread_current()->next_fd;
  thread_current()->next_fd++;
  list_push_back(&thread_current()->files, &fd_struct->elem);

  return fd_struct->fd;
}

int filesize(int fd) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  int size = file_length(fd_struct->file);
  lock_release(&files_lock);

  return size;
}

int read (int fd, void *buffer, unsigned size) {
  if (fd == 0) {
    for (unsigned i = 0; i < size; i++) {
      ((char *)buffer)[i] = input_getc();
    }
    return size;
  }

  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);
  if (fd_struct == NULL) return -1;

  lock_acquire(&files_lock);
  int bytes_read = file_read(fd_struct->file, buffer, size);
  lock_release(&files_lock);

  return bytes_read;
}

int write (int fd, const void *buffer, unsigned size) {
  // if fd is 1, write to console using putbuf and return size
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }

  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);
  // if (fd_struct == NULL) return -1; not required in the in stanford pdf, will read the test cases to check if this is needed

  lock_acquire(&files_lock);
  int bytes_written = file_write(fd_struct->file, buffer, size);
  lock_release(&files_lock);

  return bytes_written;
}

void seek (int fd, unsigned position) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  file_seek(fd_struct->file, position);
  lock_release(&files_lock);
}

unsigned tell (int fd) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  unsigned position = file_tell(fd_struct->file);
  lock_release(&files_lock);

  return position;
}

void close (int fd) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  file_close(fd_struct->file);
  lock_release(&files_lock);

  list_remove(&fd_struct->elem);
  free(fd_struct);
}

// helper method the find the file by its file descriptor
struct file_descriptor *find_file_by_fd(int fd) {
  struct thread *cur = thread_current();
  struct list_elem *e;

  for (e = list_begin(&cur->files); e != list_end(&cur->files);
       e = list_next(e)) {
    struct file_descriptor *fd_struct =
        list_entry(e, struct file_descriptor, elem);
    if (fd_struct->fd == fd) {
      return fd_struct;
    }
  }
  return NULL;
}
