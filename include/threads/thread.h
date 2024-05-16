#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

/* ------------ added for Project.1-1 ------------ */

#define WAKEUP_TICKS_DEFAULT 0

/* ------------ added for Project.1-3 ------------ */

#define NICE_MIN -20
#define NICE_DEFAULT 0
#define NICE_MAX 20

#define RECENT_CPU_DEFAULT 0

/* 
  format : 17.14 fixed-point
  n : integer
  F : 1 << 14
  x : Fixed-Point
  y : Fixed-Point
*/
#define F 16384

/* fixed_point(고정 소수점) 산술 macro */

#define CONVERT_TO_FP(n) (n * F)
#define CONVERT_TO_INT_ZERO(x) (x / F)
#define CONVERT_TO_INT_NEAREST(x) \
  (x >= 0 ? ((x + F / 2) / F) : ((x - F / 2) / F))

#define ADD_FP(x, y) (x + y)
#define SUB_FP(x, y) (x - y)

#define ADD_INT_AND_FP(x, n) (x + (n * F))
#define SUB_INT_AND_FP(x, n) (x - (n * F))

#define MUL_FP(x, y) (((int64_t)x) * y / F)
#define MUL_INT_AND_FP(x, n) (x * n)

#define DIV_FP(x, y) (((int64_t)x) * F / y)
#define DIV_INT_AND_FP(x, n) (x / n)

/* ----------------------------------------------- */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
  /* Owned by thread.c. */
  tid_t tid;                 /* Thread identifier. */
  enum thread_status status; /* Thread state. */
  char name[16];             /* Name (for debugging purposes). */
  int priority;              /* Priority. */
  /* Shared between thread.c and synch.c. */
  struct list_elem elem; /* List element. */

  /* ----------- added for project.1-1 ----------- */
  int64_t wakeup_ticks;

  /* ----------- added for project.1-2 ----------- */
  unsigned initial_priority; /* 상속 받기전 origin_priority */
  struct lock *wait_on_lock; /* 현재 쓰레드가 대기중인 lock */
  struct list donations;     /* priority를 상속해준 기부자(thread) */
  struct list_elem donation_elem; /* struct donations list를 위한 elem */

  /* ----------- added for project.1-2 ----------- */
  /* CPU사용을 얼마나 양보했는가를 표현하는 정수
     nice lower : decrease priority
     nice higher : increase priority
     즉, 양보를 많이했다는 것은 우선순위가 낮다는 얘기다.
  */
  int nice;
  int recent_cpu;

  /* --------------------------------------------- */

#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
  /* Table for whole virtual memory owned by thread. */
  struct supplemental_page_table spt;
#endif

  /* Owned by thread.c. */
  struct intr_frame tf; /* Information for switching */
  unsigned magic;       /* Detects stack overflow. */
};

/* ----------- added for Project.1 ----------- */

/* Alarm Clock */
bool is_idle_thread(struct thread *t);
void thread_set_wakeup_ticks(struct thread *t, int64_t ticks);
void thread_sleep(int64_t ticks);
void thread_check_awake(int64_t ticks);

/* ----------- added for Project.1-2 ----------- */

bool priority_ascending_sort(const struct list_elem *a,
                             const struct list_elem *b, void *aux);
void preempt_schedule(void);
void print_priority(struct list *list);

/* -------------------------------------------- */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

#endif /* threads/thread.h */
