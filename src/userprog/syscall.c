#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall = get_arg((int *)f->esp);

  switch (syscall)
  {
  case SYS_WAIT:
    /* code */
    break;
  case SYS_EXEC:
    /* code */
    break;
  case SYS_WRITE:
    /* code */
    break;
  case SYS_EXIT:
    /* code */
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

void exit_wrapper(int status) {
  // To be implemented...
}
