#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/syscall.h" /* added for PROJECT.2-2 */
#include "userprog/tss.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup(void);
static bool load(const char *file_name, struct intr_frame *if_);
static void initd(void *f_name);
static void __do_fork(void *);

/* --------------- added for PROJECT.2-1 --------------- */

static void fdt_cleanup() {
  struct thread *current = thread_current();
  file_close(current->running_file);
  current->running_file = NULL;
  while (!list_empty(&current->fdt)) {
    struct list_elem *e = list_pop_front(&current->fdt);
    struct fd_elem *tmp = list_entry(e, struct fd_elem, elem);
    struct file *file_ptr = tmp->file_ptr;
    file_close(file_ptr);
    free(tmp);
  }
}

/**
 * @brief 명령줄을 받아서 argv와 argc를 저장한다.
 * 
 * @param command_line 명령줄
 * @param argv 명령줄을 저장할 배열
 * @param argc 명령줄의 길이를 저장할 variable
*/
void parse_argument(char *command_line, char *argv[], int *argc) {
  char *token, *save_ptr = NULL;
  int i = 0;

  ASSERT(command_line != NULL);
  ASSERT(argv != NULL);
  ASSERT(argc != NULL);

  for (token = strtok_r(command_line, " ", &save_ptr); token != NULL;
       token = strtok_r(NULL, " ", &save_ptr)) {
    argv[i++] = token;
  }

  argv[i] = NULL; /* argv의 끝을 표시한다. */
  *argc = i;
}

/**
 * @brief command line으로 부터 받은 arg(인수)를 user stack에 저장한다.
 * 
 * @param argv 인수 배열
 * @param argc 인수의 개수
 * @param _if 현재 thread의 intr_frame
*/
void push_argment_stack(char *argv[], int argc, struct intr_frame *_if) {
  int i, j;
  int total_length = 0;
  char *arg_addr[(LOADER_ARGS_LEN / 2) + 1];

  /* 인수(args)를 스택에 저장한다. */
  for (i = argc - 1; i >= 0; i--) {
    _if->rsp -= strlen(argv[i]) + 1; /* null종단 문자를 추가 해야한다. */
    memcpy(_if->rsp, argv[i], strlen(argv[i]) + 1);
    arg_addr[i] = _if->rsp;
    total_length += strlen(argv[i]) + 1;
  }

  /* 8바이트 정렬을 위한 argment padding을 추가한다 */
  int padding = 8 - (total_length % 8);
  while (padding > 0) {
    _if->rsp -= sizeof(uint8_t);
    memset(_if->rsp, 0, sizeof(uint8_t));
    padding--;
  }

  /* 인수의 주소를 스택에 저장한다. */
  for (i = argc; i >= 0; i--) {
    if (i == argc) {
      _if->rsp -= sizeof(char *);
      memset(_if->rsp, 0, sizeof(char *));
      continue;
    }

    _if->rsp -= sizeof(char *);
    memcpy(_if->rsp, &arg_addr[i], sizeof(char *));
  }

  /* return address(fake address)를 스택에 저장한다. */
  _if->rsp -= sizeof(void *);
  memset(_if->rsp, 0, sizeof(void *));

  _if->R.rdi = argc;
  /* argv를 가르켜야하기기에 마지막 return address 만큼 밀어줘야 한다. */
  _if->R.rsi = _if->rsp + sizeof(void *);
}

/* --------------- added for PROJECT.2-2 --------------- */

/**
 * @brief 파일을 열고 file 구조체를 반환한다.
 * 
 * @param file file object pointer
*/
// int process_add_file(struct file *file) {
//   struct thread *curr_t = thread_current();

//   if (MAX_FD_CNT <= curr_t->next_fd) return -1; /* file descriptor table full */

//   curr_t->fdt[curr_t->next_fd] = file;
//   curr_t->next_fd++;

//   return curr_t->next_fd - 1;
// }

/**
 * @brief fd에 해당하는 file을 반환한다.
 * 
 * @param fd file descriptor
*/
// struct file *process_get_file(int fd) {
//   struct thread *curr_t = thread_current();
//   struct file *file;

//   if (fd > curr_t->next_fd) return NULL;

//   file = curr_t->fdt[fd];

//   if (file == NULL) return NULL;

//   return file;
// }

/**
 * @brief fd에 해당하는 file을 닫는다.
 * 
 * @param fd file descriptor
*/
// void process_close_file(int fd) {
//   struct thread *curr_t = thread_current();
//   struct file *file;

//   file = curr_t->fdt[fd];

//   if (file == NULL) return; /* 이미 닫힌 파일이라면 return */

//   // file_close(file); /* file close */

//   curr_t->fdt[fd] = NULL; /* file에 대한 fd 초기화 */
// }

/**
 * @brief pid에 해당하는 child process를 반환한다.
 * 
 * @param pid 찾고자하는 child process의 pid
 * 
 * @return struct thread* child process
*/
struct thread *find_child_process(int pid) {
  struct thread *parent_t = thread_current();
  struct thread *child_t;
  struct list_elem *e;

  for (e = list_begin(&parent_t->child_list);
       e != list_end(&parent_t->child_list); e = list_next(e)) {
    child_t = list_entry(e, struct thread, child_elem);

    if (pid == child_t->tid) return child_t;
  }

  if (e == list_end(&parent_t->child_list)) return TID_ERROR;

  return TID_ERROR;
}

/**
 * @brief child process를 제거한다.
 * 
 * @param t 제거할 child process
*/
void remove_child_process(struct thread *t) {
  list_remove(&t->child_elem);

  palloc_free_page(t);
}

/* ----------------------------------------------------- */

/* General process initializer for initd and other process. */
static void process_init(void) { struct thread *current = thread_current(); }

/**
 * @brief deamon process 혹은 모든 프로세스의 부모 프로세스(or 최상위 프로세스)를 생성하고 초기화한다.
 * 
 * @details daemon process : OS의 기능을 보조하고 확장하는 역할
 *                         : 사용자의 개입없이 서비스나 작업을 수행 (on background)
 *          터미널에 pstree 명령어를 이용해 process 구조를 확인할 수 있고 daemon이 최상단에 있다.
 *          
 * @note Starts the first userland program, called "initd", loaded from
 *       FILE_NAME. The new thread may be scheduled (and may even exit)
 *       before process_create_initd() returns. Returns the initd's
 *       thread id, or TID_ERROR if the thread cannot be created.
 *       Notice that THIS SHOULD BE CALLED ONCE.
 * 
 *       >  "initd"라 불리는첫 번째 userland 프로그램을 FILE_NAME에서 로드하여 시작합니다.
 *       IMO : initd는 daemon을 초기화하는데 사용자영역에서 시작하므로 userland
 *             프로그램으로 보는듯하다.
 *       >  새로운 스레드는 process_create_initd()가 반환되기 전에 스케줄링될 수 있으며
 *       >  종료될 수 있습니다. initd의 스레드 ID를 반환하거나 스레드를 생성할 수 없는 경우
 *       >  TID_ERROR를 반환합니다.
 *       >  ⛔️ 이것은 한 번만 호출되어야 합니다.
 * 
*/
tid_t process_create_initd(const char *file_name) {
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL) return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);

  /* --------------- added for PROJECT.2-1 --------------- */
  // command line의 첫번째 토큰을 thread_name으로 저장한다.
  char *ptr;
  file_name = strtok_r(file_name, " ", &ptr);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create(file_name, PRI_DEFAULT, initd, fn_copy);
  if (tid == TID_ERROR) palloc_free_page(fn_copy);
  return tid;
}

/**
 * @brief 첫 번째(모든 user process를 통틀어서) 사용자 프로세스를 시작할때
 *        thread_create로 전달해줄 thread function
 * 
 * @ref process_create_initd -> detail daemon process 참조
 * 
 * @param f_name command line
 * 
 * @note A thread function that launches first user process.
 *       >  첫번째 user process를 시작하는 thread 함수
*/
static void initd(void *f_name) {
#ifdef VM
  supplemental_page_table_init(&thread_current()->spt);
#endif

  process_init(); /* running thread */

  if (process_exec(f_name) < 0) PANIC("Fail to launch initd\n");
  NOT_REACHED();
}

struct fork_frame {
  struct intr_frame if_;
  struct thread *parent;
};

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
/**
 * @brief 자식 프로세스(부모와 같은 )를 생성하고 tid(thread id)를 반환한다.
 * 
 * @param name child process name
 * @param if_ parent intr_frame (부모 프로세스의 register 정보를 저장하기 위해)
 * 
 * @return tid_t new process's thread id
 * 
 * @note Clones the current process as `name`. Returns the new process's
 *       thread id, or TID_ERROR if the thread cannot be created.
*/
tid_t process_fork(const char *name, struct intr_frame *if_ UNUSED) {
  /* Clone current thread to new thread.*/
  return thread_create(name, PRI_DEFAULT, __do_fork, thread_current());
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool duplicate_pte(uint64_t *pte, void *va, void *aux) {
  struct thread *current = thread_current();
  struct thread *parent = (struct thread *)aux;
  void *parent_page;
  void *newpage;
  bool writable;

  /* 1. TODO: If the parent_page is kernel page, then return immediately. */
  if (is_kernel_vaddr(va)) return true;

  /* 2. Resolve VA from the parent's page map level 4. */
  parent_page = pml4_get_page(parent->pml4, va);
  if (!parent_page) return false;

  /* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
  newpage = palloc_get_page(PAL_USER);
  if (newpage == NULL) return false;

  /* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
  memcpy(newpage, parent_page, PGSIZE);
  writable = is_writable(pte);

  /* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
  if (!pml4_set_page(current->pml4, va, newpage, writable)) {
    /* 6. TODO: if fail to insert page, do error handling. */
    palloc_free_page(newpage);
    return false;
  }
  return true;
}
#endif

/**
 * @brief 부모의 실행 컨텍스트를 복사하는 thread function
 * 
 * @param aux parent thread (struct thread*)
 * 
 * @note A thread function that copies parent's execution context.
 * 
 *       > 부모의 실행 컨텍스트를 복사하는 thread function이다.
 *        
 *       Hint) parent->tf does not hold the userland context of the process.        
 *       That is, you are required to pass second argument of process_fork to
 *       this function
 * 
 *       >  Hint) parent->tf는 프로세스의 userland context를 보유하지 않습니다.
 *       >  즉, process_fork()의 두 번째 인수를 이 함수에 전달해야합니다.
*/
static void __do_fork(void *aux) {
  struct intr_frame if_;
  struct thread *parent = (struct thread *)aux;
  struct thread *current = thread_current();
  /* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
  struct intr_frame *parent_if = &parent->parent_if;

  /* 1. Read the cpu context to local stack. */
  memcpy(&if_, parent_if, sizeof(struct intr_frame));

  /* 2. Duplicate PT */
  current->pml4 = pml4_create();
  if (current->pml4 == NULL) goto error;

  process_activate(current);
#ifdef VM
  supplemental_page_table_init(&current->spt);
  if (!supplemental_page_table_copy(&current->spt, &parent->spt)) goto error;
#else
  /* ref.1) */
  if (!pml4_for_each(parent->pml4, duplicate_pte, parent)) goto error;
#endif

  /* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.
   * 
   * >  여기서부터 code 를 작성합니다.
   * >  file object를 복제하려면 include/filesys/file.h에있는 `file_duplicate`를
   * >  사용하세요. NOTE : 부모는 이 함수가 성공적으로 부모의 리소스를 복제 할 때까지
   * >  fork()에서 반환해서는 안됩니다.
   * */

  /* ----------- added for PROJECT.2-2 (fork) ----------- */

  current->next_fd = parent->next_fd;
  for (struct list_elem *e = list_begin(&parent->fdt);
       e != list_end(&parent->fdt); e = list_next(e)) {
    struct fd_elem *tmp = list_entry(e, struct fd_elem, elem);
    struct file *dup_file = file_duplicate(tmp->file_ptr);
    if (dup_file == NULL) goto error;

    struct fd_elem *e = (struct fd_elem *)malloc(sizeof(struct fd_elem));
    if (e == NULL) goto error;

    e->file_ptr = dup_file;
    e->fd = tmp->fd;
    list_push_back(&current->fdt, &e->elem);
  }
  current->running_file = file_duplicate(parent->running_file);

  process_init();
  if_.R.rax = 0;

  sema_up(&current->fork_sema);
  /* Finally, switch to the newly created process. */
  do_iret(&if_);
error:
  current->exit_status = TID_ERROR;
  // Put this here to prevent bugs for multi-oom, needs to cleanup before fork returns.
  fdt_cleanup();
  sema_up(&current->fork_sema);
  thread_exit();
}

/**
 * @brief 현재 실행 컨텍스트를 f_name으로 전환한다.
 * 
 * @param f_name command line
 * 
 * @return int FAIL ? 0 : -1
 * 
 * @note Switch the current execution context to the f_name.
*/
int process_exec(void *f_name) {
  char *file_name = f_name;
  bool success;

  /* --------------- added for PROJECT.2-1  ---------------
     parsing argument(command line)
     - argc : args count
     - argv : args array (인수는 공백으로 구분된다 + 마지막엔 NULL이 들어간다.)
       - argv[0] : file_name (프로그램 이름)
       - argv[1 ~ n] : args */
  int argc;
  char *argv[(LOADER_ARGS_LEN / 2) + 1];

  parse_argument(f_name, argv, &argc);

  /* ----------------------------------------------------- */

  /* We cannot use the intr_frame in the thread structure.
	   This is because when current thread rescheduled,
	   it stores the execution information to the member. 

     > thread 구조체에서 intr_frame을 사용할 수 없다.
     > 왜냐하면 현재 thread가 재스케줄링되면 실행 정보를 멤버에 저장하기 때문이다.*/
  struct intr_frame _if;
  _if.ds = _if.es = _if.ss = SEL_UDSEG;
  _if.cs = SEL_UCSEG;
  _if.eflags = FLAG_IF | FLAG_MBS;

  /* We first kill the current context */
  process_cleanup();

  /* And then load the binary */
  lock_acquire(&filesys_lock);
  success = load(argv[0], &_if);
  lock_release(&filesys_lock);

  /* If load failed, quit. */
  if (!success) return -1;

  /* --------------- added for project.2-1 --------------- */

  /* token(args)를 user Stack에 저장한다. */
  push_argment_stack(argv, argc, &_if);

  /* 😉 주석을 해제하면 argment가 stack에 들어가있는지 확인할 수 있다.
  hex_dump((uintptr_t)_if.rsp, _if.rsp, USER_STACK - _if.rsp, true); */

  /* ----------------------------------------------------- */

  /* Start switched process. */
  do_iret(&_if); /* Start the process by iret. */
  palloc_free_page(file_name);
  NOT_REACHED();
}

/**
 * @brief 🟢 thread가 exit될 때까지 **기다리고(block)** 종료 상태를 반환한다.
 * 
 * @note  Waits for thread TID to die and returns its exit status.
 * 
 *        >  TID가 종료될 때까지 기다리고 종료 상태를 반환합니다.
 * 
 *        If it was terminated by the kernel (i.e. killed due to an
 *        exception), returns -1.
 * 
 *        >  커널에 의해 종료되었으면(즉, 예외로 인해 종료되었으면) -1을 반환합니다.
 * 
 *        If TID is invalid or if it was not a child of the calling
 *        process, or if process_wait() has already been successfully
 *        called for the given TID, returns -1 immediately, without 
 *        waiting
 * 
 *        >  TID가 유효하지 않거나 호출 프로세스의 자식이 아니거나,
 *        >  process_wait()가 이미 주어진 TID에 대해 성공적으로 호출되었거나
 *        >  즉시 -1을 반환하고 기다리지 않습니다. 
*/
int process_wait(tid_t child_tid UNUSED) {
  /* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

  /* --------------- added for PROJECT.2-2(Hierarchy)  --------------- */
  struct thread *cur_thread = thread_current();
  int child_status = -1;

  struct thread *child_thread;
  for (struct list_elem *e = list_begin(&cur_thread->child_list);
       e != list_end(&cur_thread->child_list); e = list_next(e)) {
    child_thread = list_entry(e, struct thread, child_elem);
    if (child_thread->tid == child_tid) {
      sema_down(&child_thread->load_sema);
      list_remove(&child_thread->child_elem);
      child_status = child_thread->exit_status;
      sema_up(&child_thread->exit_sema);
      break;
    }
  }

  return child_status;

  /* --------------- before for PROJECT.2-2(Hierarchy)  --------------- 
  return -1; */
}

/**
 * @brief 프로세스를 종료하고 프로세스의 자원을 정리한다.
 * 
 * @note  Exit the process. This function is called by thread_exit().
 * 
 *        >  프로세스를 종료한다. 이 함수는 thread_exit()에 의해 호출됩니다.
*/
void process_exit(void) {
  struct thread *curr_t = thread_current();
  struct thread *child_t = NULL;
  /* TODO: Your code goes here.
	 * TODO: Implement process termination message 
	 * TODO: (see project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

  /* --------------- added for PROJECT.2-2 --------------- */
  struct file *file;

  /* CLOSE : 열려있는 file을 모두 닫고 fd Table을 deallocate ! */
  fdt_cleanup();

  for (struct list_elem *e = list_begin(&curr_t->child_list);
       e != list_end(&curr_t->child_list); e = list_next(e)) {
    child_t = list_entry(e, struct thread, child_elem);
    if (child_t->parent_t == curr_t) {
      child_t->parent_t = NULL;
    }
  }

  /* parent의 process_wait()를 release ! */
  sema_up(&curr_t->load_sema);

  /* parent의 list_remove를 wait ! */
  sema_down(&curr_t->exit_sema);
  process_cleanup();

  /* ------------------------------------------------------ */
}
/**
 * @brief 현재 프로세스의 자원을 해제(free)한다.
 * 
 * @note  Frees the current process's resources.
*/
static void process_cleanup(void) {
  struct thread *curr = thread_current();

  if (curr->running_file) file_close(curr->running_file);
  curr->running_file = NULL;

#ifdef VM
  supplemental_page_table_kill(&curr->spt);
#endif

  uint64_t *pml4;
  /* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
  pml4 = curr->pml4;
  if (pml4 != NULL) {
    /* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
    curr->pml4 = NULL;
    pml4_activate(NULL);
    pml4_destroy(pml4);
  }
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void process_activate(struct thread *next) {
  /* Activate thread's page tables. */
  pml4_activate(next->pml4);

  /* Set thread's kernel stack for use in processing interrupts. */
  tss_update(next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
  unsigned char e_ident[EI_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
};

struct ELF64_PHDR {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack(struct intr_frame *if_);
static bool validate_segment(const struct Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/**
 * @brief ELF 파일을 읽고 intr_frame->rsp, rip를 초기화한다.
 * 
 * @param file_name ELF 파일 이름
 * @param if_ 현재 thread의 intr_frame 
 * 				(🟢 user process가 사용한 레지스터를 저장하는 구조체)
 * 
 * @note Loads an ELF executable from FILE_NAME into the current thread.
 * 			 Stores the executable's entry point into *RIP
 * 			 and its initial stack pointer into *RSP.
 * 			 Returns true if successful, false otherwise.
 * 
 * 			 >  1) FILE_NAME에서 ELF 실행 파일을 현재 스레드로 로드합니다.
 * 			 >  2) 실행 파일의 진입 지점을 *RIP에 저장합니다
 * 			 >  3) 초기 스택 포인터를 *RSP에 입력합니다.
 * 			 >  4) 성공하면 true, 그렇지 않으면 false를 반환합니다.
 * 
 * @details rip : Return Instruction Pointer
 * 					  -> 다음에 실행될 즉, 실행이 완료된 명령 이후의 명령어의 주소
 * 
 * 				  rsp : Return Stack Pointer
 * 						-> 현재 실행중인 process의 stack 최상단 주소 
 * 							 즉, 지역변수, 함수호출등이 저장
 * 
 * 					user process에서 int 와 같은 Trap을 발동시키면
 * 					1. (rsp -> user stack) to (rsp->kernal stack)
 * 					2. kernal stack에 user가 사용한 레지스터의 상태가 저장된다.
 * 					user가 사용한 데이터의 시작점인 rsp는 데이터 struct인 intr_frame->rsp에 저장된다.
 * 
 * 					❌ user process Stack의 데이터를 저장하는것이 아니다.
*/
static bool load(const char *file_name, struct intr_frame *if_) {
  struct thread *t = thread_current();
  struct ELF ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pml4 = pml4_create();
  if (t->pml4 == NULL) {
    printf("pml create fail : %d", thread_current()->tid);
    goto done;
  }
  process_activate(thread_current());

  /* Open executable file. */
  file = filesys_open(file_name);
  if (file == NULL) {
    printf("load: %s: open failed\n", file_name);
    goto done;
  }
  t->running_file = file;
  file_deny_write(file);

  /* 🟠 Read and verify executable header.
		 1. file의 ELF헤더를 읽고 쓴다. file_read()
		 2. ELF header의 예외처리를 수행한다. */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr ||
      memcmp(ehdr.e_ident, "\177ELF\2\1\1", 7) || ehdr.e_type != 2 ||
      ehdr.e_machine != 0x3E  // amd64
      || ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Phdr) ||
      ehdr.e_phnum > 1024) {
    printf("load: %s: error loading executable\n", file_name);
    goto done;
  }

  /* 🟠 Read program headers.
		 1. file의 program 헤더를 읽고 쓴다. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) {
    struct Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length(file)) goto done;
    file_seek(file, file_ofs);

    if (file_read(file, &phdr, sizeof phdr) != sizeof phdr) goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type) {
      case PT_NULL:
      case PT_NOTE:
      case PT_PHDR:
      case PT_STACK:
      default:
        /* Ignore this segment. */
        break;
      case PT_DYNAMIC:
      case PT_INTERP:
      case PT_SHLIB:
        goto done;
      case PT_LOAD:
        if (validate_segment(&phdr, file)) {
          bool writable = (phdr.p_flags & PF_W) != 0;
          uint64_t file_page = phdr.p_offset & ~PGMASK;
          uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
          uint64_t page_offset = phdr.p_vaddr & PGMASK;
          uint32_t read_bytes, zero_bytes;
          if (phdr.p_filesz > 0) {
            /* Normal segment.
						 * Read initial part from disk and zero the rest. */
            read_bytes = page_offset + phdr.p_filesz;
            zero_bytes =
                (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
          } else {
            /* Entirely zero.
						 * Don't read anything from disk. */
            read_bytes = 0;
            zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
          }
          if (!load_segment(file, file_page, (void *)mem_page, read_bytes,
                            zero_bytes, writable))
            goto done;
        } else
          goto done;
        break;
    }
  }

  /* 🟠 Set up stack.
		 user stack을 초기화한다. 함수 내부에서 stack pointer의 시작점을 초기화한다. */
  if (!setup_stack(if_)) goto done;

  /* Start address.
		 프로그램의 시작점을 설정한다. 즉, 메모리에 로드되어 CPU가 어디부터 실행할지 알 수 있다 */
  if_->rip = ehdr.e_entry;

  /* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html).
   *
   * ✅ DONE : load에서 argumnet passing하기 위해서는 param으로 command_line을
   *           받아야 했는데 단순히 file_name만 넘겨주기 위해 process_exec()에서
   *           parsing을 완료했다. */

  success = true;

done:
  /* We arrive here whether the load is successful or not. */
  // file_close(file);
  /* We arrive here whether the load is successful or not. */
  if (file != t->running_file) {
    file_close(file);
  }
  return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Phdr *phdr, struct file *file) {
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (uint64_t)file_length(file)) return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0) return false;

  /* The virtual memory region must both start and end within the
	   user address space range. */
  if (!is_user_vaddr((void *)phdr->p_vaddr)) return false;
  if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz))) return false;

  /* The region cannot "wrap around" across the kernel virtual
	   address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr) return false;

  /* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE) return false;

  /* It's okay. */
  return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page(void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 * 
 * >  FILE에서 OFS에서 시작하는 세그먼트를 UPAGE 주소에 로드합니다.
 * >  총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리가 초기화됩니다.
 * >  다음과 같이 :
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 * 
 * >  UPAGE의 READ_BYTES 바이트는 OFS에서 시작하는 FILE에서 읽어야합니다.
 * >  UPAGE + READ_BYTES의 ZERO_BYTES 바이트는 0으로 설정되어야합니다.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 * 
 * >  이 함수에 의해 초기화된 페이지는 WRITABLE이 true이면 사용자 프로세스에 의해
 * >  쓰기 가능해야하며 그렇지 않으면 읽기 전용이어야합니다.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs.
 *  
 * >  성공하면 true, 메모리 할당 오류 또는 디스크 읽기 오류가 발생하면 false를 반환합니다. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable) {
  ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(ofs % PGSIZE == 0);

  file_seek(file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) {
    /* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Get a page of memory. */
    uint8_t *kpage = palloc_get_page(PAL_USER);
    if (kpage == NULL) return false;

    /* Load this page. */
    if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes) {
      palloc_free_page(kpage);
      return false;
    }
    memset(kpage + page_read_bytes, 0, page_zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_page(upage, kpage, writable)) {
      printf("fail\n");
      palloc_free_page(kpage);
      return false;
    }

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
  }
  return true;
}

/**
 * @brief USER_STACK을 page에 할당하고 0으로 초기화한다.
 * 
 * @note Create a minimal stack by mapping a zeroed page at the USER_STACK
*/
static bool setup_stack(struct intr_frame *if_) {
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if (kpage != NULL) {
    success = install_page(((uint8_t *)USER_STACK) - PGSIZE, kpage, true);
    if (success)
      if_->rsp = USER_STACK;  // 0x47480000
    else
      palloc_free_page(kpage);
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool install_page(void *upage, void *kpage, bool writable) {
  struct thread *t = thread_current();

  /* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
  return (pml4_get_page(t->pml4, upage) == NULL &&
          pml4_set_page(t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

/** 
 * @brief load_segment() 로 조각낸 page의 file(아직 load되지않음)을 physical
 *        memory에 올린다. 
 * 
 * @param page load할 kva(PA)를 알고있는 page
 * @param aux load_segment() 에서 vm_alloc_page_with_initializer()로 넘겨준 aux
 * 
 * @see load_segment(), vm_alloc_page_with_initializer()
 * 
 * TODO: Load the segment from the file 
 * TODO: This called when the first page fault occurs on address VA. 
 * TODO: VA is available when calling this function. 
 * 
 *  >  file에서 segment를 load하세요.
 *  >  이 함수는 VA 주소에서 처음 page가 발생할 때 호출됩니다.
 *  >  이 함수를 호출할 때 VA를 사용할 수 있습니다. 
*/
static bool lazy_load_segment(struct page *page, void *aux) {
  struct file_segment_info *info = (struct file_segment_info *)aux;
  void *physical_addr = page->frame->kva;
  bool result = false;

  struct file *file = info->file;
  off_t offset = info->offset;
  uint32_t read_bytes = info->read_bytes;
  uint32_t zero_bytes = info->zero_bytes;

  /* file의 offset에서 read_bytes만큼 읽어서 physical_addr에 저장한다. */
  off_t actually_read_bytes =
      file_read_at(file, physical_addr, read_bytes, offset);

  /* file에서 실제 읽은 bytes와 읽어야할 bytes가 다르다면 throw */
  if ((uint32_t)actually_read_bytes != read_bytes) {
    free(aux);
    goto done;
  }

  /* page의 나머지 부분을 0으로 초기화한다. */
  memset(physical_addr + actually_read_bytes, 0, zero_bytes);
  result = true;

done:
  return result;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable) {
  ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(ofs % PGSIZE == 0);

  while (read_bytes > 0 || zero_bytes > 0) {
    /* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes.
     * 
     * >  이 페이지를 어떻게 채울지 계산하십시오.
     * >  FILE에서 PAGE_READ_BYTES 바이트를 읽을 것입니다.
     * >  그리고 마지막 PAGE_ZERO_BYTES 바이트를 0으로 설정합니다. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* TODO: Set up aux to pass information to the lazy_load_segment. */
    struct file_segment_info *aux = calloc(1, sizeof(struct file_segment_info));
    if (aux == NULL) return false;

    aux->file = file_duplicate(file);
    aux->read_bytes = page_read_bytes;
    aux->zero_bytes = page_zero_bytes;
    aux->offset = ofs;

    if (!vm_alloc_page_with_initializer(VM_ANON, upage, writable,
                                        lazy_load_segment, aux))
      return false;

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
    ofs += page_read_bytes;
  }
  return true;
}

/**
 * @brief pending page를 할당하고 해당 page를 USER_STACK에 매핑한다.(실제 physical 
 *        memory에 올라간다. 단, USER_STACK은 va다.)
 * 
 * @note Create a PAGE of stack at the USER_STACK. Return true on success.
 * 
 *       >  USER_STACK에 PAGE(physical mem)를 생성한다. 성공하면 true를 반환한다.
 * 
 * @details 4kb 만큼 밀어주고 USER_STACK으로 다시 위를 가르킨다
 *          즉, 처음은 anon page이다.
*/
static bool setup_stack(struct intr_frame *if_) {
  bool success = false;
  void *stack_bottom = (void *)(((uint8_t *)USER_STACK) - PGSIZE);

  /* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. 
   * 
   * >  stack_bottom에 stack을 매핑하고 즉시 페이지를 요청하십시오.
   * >  성공하면 rsp를 적절하게 설정하십시오.
   * >  페이지가 stack임을 표시해야합니다.
   */

  /* pending 상태의 page를 할당한다. */
  if (vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, true)) {
    if_->rsp = USER_STACK;
    success = true;
  }

  /* pending 상태의 page를 요구하고 physical memory에 mapping한다. */
  if (vm_claim_page(stack_bottom)) {
    success = true;
  }

  if_->rsp = USER_STACK;

  return success;
}
#endif /* VM */
