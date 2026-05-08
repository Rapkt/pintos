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
  char *buffer;
  int size;
  char *file;
  char *cmd_line;
  switch (syscall) {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    exit(get_arg((int *)f->esp + 1));
    break;
  case SYS_EXEC:
    cmd_line = get_ptr_arg((int *)f->esp + 1);
    validate_str(cmd_line); // Validate the string before executing!
    f->eax = exec(cmd_line);
    break;
  case SYS_WAIT:
    f->eax = wait(get_arg((int *)f->esp + 1));
    break;
  case SYS_CREATE: {
    file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    unsigned initial_size = get_arg((int *)f->esp + 2);
    f->eax = create(file, initial_size);
  } break;
  case SYS_REMOVE: {
    file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    f->eax = remove(file);
  } break;
  case SYS_OPEN: {
    file = get_ptr_arg((int *)f->esp + 1);
    validate_str(file);
    f->eax = open(file);
  } break;
  case SYS_FILESIZE:
    fd = get_arg((int *)f->esp + 1);
    f->eax = filesize(fd);
    break;
  case SYS_READ:
    fd = get_arg((int *)f->esp + 1);
    buffer = get_ptr_arg((int *)f->esp + 2);
    size = get_arg((int *)f->esp + 3);
    validate_buffer(buffer, size);
    f->eax = read(fd, buffer, size);
    break;
  case SYS_WRITE:
    fd = get_arg((int *)f->esp + 1);
    buffer = get_ptr_arg((int *)f->esp + 2);
    size = get_arg((int *)f->esp + 3);
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
    exit(-1);
    break;
  }

  // printf("system call!\n");
  // thread_exit();
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
  check_buffer(buffer, size);
}

// Gamal: validates pointer to input, if valid it derefernces it and returns it
// Gamal: Example use: get_arg((int *)fd->esp + i), where 'i' is the agrument
// number. ya3ny lw 3ayz awl arg, yeb2a 1, lw tany arg yeb2a 2...
int get_arg(const int *ptr) {
  check_buffer((void *)ptr, 4);
  return *ptr;
}
void *get_ptr_arg(const void *ptr) {
  /* ptr is pointer to a stack word that holds an address. */
  check_buffer((void *)ptr, 4);
  void *addr = *(void *const *)ptr;
  check_ptr(addr);
  return addr;
}

// Gamal: validates whether pointer is valid or not
void check_ptr(const void *ptr) { check_buffer(ptr, 1); }

void check_buffer(const void *ptr, unsigned size) {
  for (int i = 0; i < size; i++) {
    const char *p = (const char *)ptr + i;
    if (p == NULL || !is_user_vaddr(p) ||
        pagedir_get_page(thread_current()->pagedir, p) == NULL) {
      exit(-1);
    }
  }
}

void halt(void) { shutdown_power_off(); }

void exit(int status) {
  thread_current()->exit_status = status;
  thread_exit();
}

// void exit(int status) { thread_exit(); }

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

int read(int fd, void *buffer, unsigned size) {
  if (fd == 0) {
    for (unsigned i = 0; i < size; i++) {
      ((char *)buffer)[i] = input_getc();
    }
    return size;
  }

  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);
  if (fd_struct == NULL)
    return -1;

  lock_acquire(&files_lock);
  int bytes_read = file_read(fd_struct->file, buffer, size);
  lock_release(&files_lock);

  return bytes_read;
}

int write(int fd, const void *buffer, unsigned size) {
  // if fd is 1, write to console using putbuf and return size
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }

  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);
  if (fd_struct == NULL)
    exit(-1); // if fd is invalid, exit with error status

  lock_acquire(&files_lock);
  int bytes_written = file_write(fd_struct->file, buffer, size);
  lock_release(&files_lock);

  return bytes_written;
}

void seek(int fd, unsigned position) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  file_seek(fd_struct->file, position);
  lock_release(&files_lock);
}

unsigned tell(int fd) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);

  lock_acquire(&files_lock);
  unsigned position = file_tell(fd_struct->file);
  lock_release(&files_lock);

  return position;
}

void close(int fd) {
  // find the file descriptor struct corresponding to the given fd
  struct file_descriptor *fd_struct = find_file_by_fd(fd);
  if (fd_struct == NULL)
    exit(-1); // if fd is invalid, exit with error status

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
