#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h> /* added for PROJECT.2-1 */

void syscall_init(void);

/* --------------- added for PROJECT.2-1 --------------- */

void check_user_address(const void *addr);

/* --------------- System Calls  --------------- */

void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);

/* ----------------------------------------------------- */

#endif /* userprog/syscall.h */
