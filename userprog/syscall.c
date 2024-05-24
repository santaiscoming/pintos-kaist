#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/thread.h"
#include "userprog/gdt.h"

// struct gp_registers {
// 	uint64_t r15;
// 	uint64_t r14;
// 	uint64_t r13;
// 	uint64_t r12;
// 	uint64_t r11;
// 	uint64_t r10;
// 	uint64_t r9;
// 	uint64_t r8;
// 	uint64_t rsi;
// 	uint64_t rdi;
// 	uint64_t rbp;
// 	uint64_t rdx;
// 	uint64_t rcx;
// 	uint64_t rbx;
// 	uint64_t rax;
// } __attribute__((packed));

// struct intr_frame {
// 	/* Pushed by intr_entry in intr-stubs.S.
// 	   These are the interrupted task's saved registers. */
// 	struct gp_registers R;
// 	uint16_t es;
// 	uint16_t __pad1;
// 	uint32_t __pad2;
// 	uint16_t ds;
// 	uint16_t __pad3;
// 	uint32_t __pad4;
// 	/* Pushed by intrNN_stub in intr-stubs.S. */
// 	uint64_t vec_no; /* Interrupt vector number. */
// /* Sometimes pushed by the CPU,
//    otherwise for consistency pushed as 0 by intrNN_stub.
//    The CPU puts it just under `eip', but we move it here. */
// 	uint64_t error_code;
// /* Pushed by the CPU.
//    These are the interrupted task's saved registers. */
// 	uintptr_t rip;
// 	uint16_t cs;
// 	uint16_t __pad5;
// 	uint32_t __pad6;
// 	uint64_t eflags;
// 	uintptr_t rsp;
// 	uint16_t ss;
// 	uint16_t __pad7;
// 	uint32_t __pad8;
// } __attribute__((packed));

/* --------------- added for PROJECT.2-1 System call --------------- */

#include "lib/user/syscall.h"

/* ----------------------------------------------------- */

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void) {
  write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 | ((uint64_t)SEL_KCSEG)
                                                               << 32);
  write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

  /* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
  write_msr(MSR_SYSCALL_MASK,
            FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/**
 * @brief System call num에 맞는 System call 호출 handler
 * 
 * @param f interrupt frame ->R.rax에 syscall_num이 저장되어있음
 * 
 * @note  The main system call interface  
 * 
 * @details passing arguments register
 *          %rdi(1st), %rsi(2nd), %rdx(3rd), %r10(4th), %r8(5th), %r9(6th)
*/

void syscall_handler(struct intr_frame *f UNUSED) {
  /* --------------- added for PROJECT.2-1 --------------- */

  int syscall_num = f->R.rax;

  switch (syscall_num) {
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT: /* int status */
      exit((int)(f->R.rdi));
      break;

      // case SYS_FORK: /* const char *thread_name */
      //   check_user_address(f->R.rdi);
      //   fork(f->R.rdi);
      //   break;

      // case SYS_EXEC: /* const char *file */
      //   check_user_address(f->R.rdi);
      //   exec(f->R.rdi);
      //   break;

      // case SYS_WAIT: /* pid_t */
      //   wait(f->R.rdi);
      //   break;

      // case SYS_CREATE: /* const char *file, unsigned initial_size */
      //   check_user_address(f->R.rdi);
      //   create(f->R.rdi, f->R.rsi);
      //   break;

      // case SYS_REMOVE: /* const char *file */
      //   check_user_address(f->R.rdi);
      //   remove(f->R.rdi);
      //   break;

      // case SYS_OPEN: /* const char *file */
      //   check_user_address(f->R.rdi);
      //   open(f->R.rdi);
      //   break;

      // case SYS_FILESIZE: /* int fd */
      //   filesize(f->R.rdi);
      //   break;

      // case SYS_READ: /* int fd, void *buffer, unsigned length */
      //   check_user_address(f->R.rsi);
      //   read(f->R.rdi, f->R.rsi, f->R.rdx);
      //   break;

      // case SYS_WRITE: /* int fd, const void *buffer, unsigned length */
      //   check_user_address(f->R.rsi);
      //   write(f->R.rdi, f->R.rsi, f->R.rdx);
      //   break;

      // case SYS_SEEK: /* int fd, unsigned position */
      //   seek(f->R.rdi, f->R.rsi);
      //   break;

      // case SYS_TELL: /* int fd */
      //   tell(f->R.rdi);
      //   break;

      // case SYS_CLOSE: /* int fd */
      //   close(f->R.rdi);
      //   break;

      // case SYS_DUP2: /* int oldfd, int newfd */
      //   dup2(f->R.rdi, f->R.rsi);
      //   break;

      // default: /* undefined syscall */
      //   exit(-1);
      //   break;
  }

  /* --------------- before PROJECT.2-1 --------------- 
  printf("system call!\n"); */

  /* ----------------------------------------------------- */

  thread_exit();
}

/* --------------- added for PROJECT.2-1 --------------- */

/**
 * @brief pointer address가 kernel address에 접근하는지 확인하고
 *        접근한다면 throw 한다.
 * 
 * @param addr pointer address
*/
void check_user_address(const void *addr) {
  if (is_kernel_vaddr(addr) || !addr) {
    exit(-1);
  }
}

void halt(void) { power_off(); }

void exit(int status) {
  struct thread *cur = thread_current();
  cur->exit_state = status;

  printf("%s: exit(%d)\n", cur->name, status);

  thread_exit();

  cur->tf.R.rax = status;
}

/* ----------------------------------------------------- */