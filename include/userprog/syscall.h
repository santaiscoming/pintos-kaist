#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* --------------- added for PROJECT.2-1 --------------- */

#include <stdbool.h>
#include <synch.h>

/* ----------------------------------------------------- */

void syscall_init(void);

/* --------------- added for PROJECT.2-1 --------------- */

void check_user_address(const void *addr);

/* --------------- System Calls  --------------- */

struct lock filesys_lock; /* read(), write()시에 동기화 문제를 위한 lock  */

void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);

/* ----------------------------------------------------- */

#endif /* userprog/syscall.h */
