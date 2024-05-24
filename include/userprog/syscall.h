#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);

/* --------------- added for PROJECT.2-1 --------------- */

void check_user_address(const void *addr);

/* --------------- System Call  --------------- */

void halt(void);
void exit(int status);

/* ----------------------------------------------------- */

#endif /* userprog/syscall.h */
