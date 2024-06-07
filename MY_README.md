# PROJECT 1

## ✅ TODO LIST

- **kernel mode** 에서 돌아가는 `int`에 대해서 정리하자
  - kaist ppt에 자세히 나와있다.

## 📌 알아두면 좋은 사항

### BLOCKED 상태와 READY 상태는 다르다.

```c
enum thread_status {
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};
```

`THREAD_BLOCKED` 의 정의를 보면 알 수 있듯이 Waiting for **trigger**

- 즉, 특정 `trigger`에 의해 대기상태가 되어있을때는 `THREAD_BLOCKED`상태인것이다.
- 반면 `ready_list`의 `waiters`는 `READY` 상태이다.

또한, `sema_down`에서는 `value`가 0이라면 `thread`를 `BLOCKED`상태로 만들고 `schdule()`을 다시한다.

- 즉, 현재쓰레드를 `semaphore`의 waiters에 넣고 `BLOCKED`상태이므로 다음 `next_thread`를 `ready_list`에 꺼내와 `컨텍스트 스위칭`한다

### 세마포어와 ready_list

세마포어와 ready_list의 상관관계는 미약하다. 아니 없을수도 있다.

ready_list란 cpu의 스케쥴링을 관리하는 도구다.

세마포어는 여러 쓰레드가 하나의 resource에 접근할때 생기는 동시성을 해결하기 위한 도구다.

#### ready_list와 waiters안에 thread 차이

- ready_list

  - CPU의 자원(메모리)을 할당받아 프로세스의 컨텍스트를 실행 준비하는 thread

- semaphore waiters
  - 하나의 리소스에 접근하려했지만 세마포어를 획득하지못해 대기중인 thread

## 📝 alarm clock

현재는 **spin lock** 형태로 구현되어있다.

다시말해서, idle thread가 대기중인 상태에서는 항상 while문 안에 갖혀있으므로 프로세스 자원 소모가 심각하다.

이를 sleep_list를 생성해 잠재운 후에 특정 시간이 지나면 깨어나도록 개선해야한다.

## 📝 priority schedule

### implement list

1. 쓰레드를 생성하고 `ready_list`에 넣을때 **ordered로 정렬**되도록 넣어야한다.
2. `실행중인 thread`가 `new_thread`보다 우선순위가 낮다면 `실행중인 thread`를 `yield` 후 `schedule()` 해야한다

### modified list

1. `thread_unblock()`을 호출할때 `ready_list`에 삽입할때 우선순위로 정렬되어야한다.
2. `thread_yield()`를 호출해 `running thread`를 `ready_queue`에 삽입할때도 우선순위 정렬되어야 한다.

   - 알아두면 좋은것 : `thread_yield` 호출시에 `schdule()`도 호출된다

3. `thread_set_priority()` 함수는 Running thread의 우선순위를 변경한다. 만약 ready_list의 가장 앞 thread의 우선순위보다 낮아진다면 rescheduling이 필요하다.

   다음은 `thread_create()` 의 일부분이다

   ```c
     // ...
     thread_unblock(new_t);

     if (thread_get_priority() < priority) thread_yield();

     return new_t_id
   ```

   `thread_unblock(new_t)`에 의문이 생길 수 있는데 이는 ready_queue에 새로 생성한 thread를 ready_queue에 넣는것이다.

### lock, semaphore

#### 🚽 semaphore

```c
/**
 * @brief 🚽 변기.
 * 				다른 실행 쓰레드들과의 동기화를 위한 그저 **타입 변수**
 * 				semaphore를 단순히 하나의 모듈 (또는 라이브러리 라고 생각하자)
 *
 * @param value 변기 개수 (사용가능한 리소스의 개수)
 * @param waiters 대기중인 사람들(대기중인 쓰레드들의 list)
*/
struct semaphore {
  unsigned value;      /* Current value. */
  struct list waiters; /* List of waiting threads. */
};

```

#### 🚻 LOCK

```c
/**
 * @brief 🚻 화장실.
 * 				lock을 통해 다른 쓰레드들과의 동기화를 위한 타입 변수
 *
 * @param holder 화장실을 잠근 사람 (현재 lock을 갖고있는 thread)
 * @param semaphore 🚽 변기. (lock에 접근이 가능한 리소스의 개수와 lock을 가지려고
 * 									대기중인 쓰레드들의 list)
*/
struct lock {
  struct thread *holder;      /* Thread holding lock (for debugging). */
  struct semaphore semaphore; /* Binary semaphore controlling access. */
};

```

## 📝 BSD

### 👉 nice

-> thread가 cpu를 얼마나 양보했는가를 나타낸다.

- `nice`가 높을수록 cpu를 많이 양보한것이다.
  - 다시말해, priority가 낮아진다.👇
- `nice`가 낮을수록 cpu를 적게 양보한것이다.
  - 다시말해, priority가 높아진다.👆

### 👉 recent_cpu

-> 각 thread의 cpu 사용시간을 반영한다.

### 👉 load_avg

-> cpu의 부하를 나타낸다.

### 👉 decay

-> 감쇄계수를 의미한다.

recent_cpu는 시간이 지남에 따라 감쇄되어야 하는데 thread가 cpu를 사용안했다고 해서 급격하게 recent_cpu가 감쇄하면 안되기에 감쇄계수를 사용한다.

### Types

- **integer**

  - priority
  - nice
  - ready_thread_cnt

- **Fixed-point**

  - recent_cpu
  - load_avg

### ✅ To-do

#### props

- [x] `nice`,`recent_cpu` property 추가

  - [x] `nice`,`recent_cpu` default 및 범위 추가

-[x] `thread_init` 수정

-[x] `thread`생성시에 `priority` **31** 로 초기화

#### function

- 기본제공함수

  - `void thread_set_nice(int nice)`
  - `int thread_get_nice(void)`
  - `int thread_get_load_avg(void)`
  - `int thread_get_recent_cpu(void)`

- calc

  - [x] `calc_priority()` using `calc_recent_cpu()`, `calc_nice()`
  - [x] `calc_recent_cpu()`
  - [x] `load_avg()`
  - [x] `increase_recent_cpu()` 1씩 증가
  - [x] 모든 쓰레드에 대해 `re_calc_priority()`, `re_calc_recent_cpu()`

- getter

  - [x] `thread_get_recnet_cpu()`
  - [x] `thread_get_load_avg()`
  - [x] `thread_get_nice()`

- setter

  - [x] `thread_set_nice()`

- modify

  - [x] `lock_acquire()`

    - forbid **donate priority**

  - [x] `lock_release()`

    - forbid **donate priority**

  - [x] `thread_set_priority()`

    - disable **Advanced scheduler**

  - [x] `timer_interrupt()`
    - EACH **1 ticks (RUNNING THREAD)** increase `recent_cpu` by **1**
    - EACH **1 SEC (ALL THREAD)** re-calculate `recent_cpu` and `load_avg`
    - EACH **4 TICKS (Global Var)** re-calculate `priority`

# 🤔 의문점

```c
void thread_sleep(int64_t ticks) {
  struct thread *curr_t = thread_current();
  enum intr_level old_level;

  old_level = intr_disable();      /* interrupt off */
  ASSERT(!is_idle_thread(curr_t)); /* check curr_t is idle */

  thread_set_wakeup_ticks(curr_t, ticks); /* ticks 설정 */

  list_push_back(&sleep_list, &curr_t->elem); /* sleep_list에 넣어준다*/
  do_schedule(THREAD_BLOCKED);

  intr_set_level(old_level); /* restore interrupt */
}
```

어째서 schedule 이전에 list_push_back을 하면 안될까 ? schedule이란 결국 다음 쓰레드로 제어권을 넘기고 현재 쓰레드에 대한 컨텍스트를 저장한뒤나에게 제어권이 넘어왔을때 중단시점부터 실행하는것을 말한다.

즉, schdule 이후에 list_push_back을 넣고 테스트를 돌렸을떄 loop를 돌면서 프로그램이 종료되는것은 현재 쓰레드를 sleep_list에 넣지 못한상태로 다음 쓰레드로 제어권이 넘어가기에 테스트가 돌아가지 않는것으로 추론된다.

# 그렇다면 sema_down도 컨텍스트가 바뀌고 멈추는가 ?

```c
void sema_down(struct semaphore *sema) {
  enum intr_level old_level;

  ASSERT(sema != NULL);
  ASSERT(!intr_context());

  old_level = intr_disable();
  while (sema->value == 0) {
    /* --------- befroe Project.1-2 ---------
    list_push_back(&sema->waiters, &thread_current()->elem); */

    /* --------- after Project.1-2 --------- */
    list_insert_ordered(&sema->waiters, &thread_current()->elem,
                        (list_less_func *)&priority_ascending_sort, NULL);

    thread_block();
  }
  sema->value--;
  intr_set_level(old_level);
}
```

block으로 컨텍스트가 스위칭되고 unblock 되었을때 다시 while문을 확인하고 value가 0이 아니면 sema를 -- 하고 lock을 취득한다.

## thread_awake

```c
void thread_check_awake(int64_t ticks) {
  struct list_elem *curr_ll_e;
  struct thread *curr_t;

  curr_ll_e = list_begin(&sleep_list);

  while (1) {
    if (curr_ll_e == list_end(&sleep_list)) break;

    curr_t = list_entry(curr_ll_e, struct thread, elem);

    if (curr_t->wakeup_ticks <= ticks) {
      curr_ll_e = list_remove(curr_ll_e);
      thread_unblock(curr_t);
      continue;
    }

    curr_ll_e = list_next(curr_ll_e);
  }
}
```

list_remove와 `thread_unblock`의 순서를 변경하면 에러가 발생한다

이유가 무엇일까 ?

### nice

nice를 변경했을때는 스케쥴링을 다시하지만 recent_cpu가 변경됐을떄에는 스케쥴링을 다시하지 않는 이유는

recent_cpu는 모든 쓰레드에대해 똑같은 값이 선형적으로 증가할 뿐더러 특정 값에 수렴하기에

recent_cpu는 스케쥴링에 변하지않는다

단, nice라는 값은 얼마나 양보해야하는지 설정하는 값 즉, priority를 결정하는 값이기에 스케쥴링을 다시한다.

허나 prioirty를 4tick마다 변경하기에 nice를 업데이트하자마자 스케쥴링을 다시하는것은 비효율적이 아닐까싶다.

# PROJECT 2

## PROJECT 2-1 "PASSING ARGMENT"

### STACK은 아래로 커진다의 의미

```c
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

 // { ... more thing }
}
```

위의 코드를 보면 user_stack에 argument를 넣는데 `rsp`를 감소시키는것을 볼 수 있다.

일반적으로 메모리에 무언가를 push 한다면 memory는 증가하는 방향으로 push된다.

그러나 stack은 누누히 설명해왔듯 아래로 커지기에 `rsp`를 감소시키는것이다.

### args

command line의 길이는 128바이트로 제한되어있다. (GITBOOK)

args의 구분은 공백으로 해준다. 즉, argv의 최대 개수는 64 + 1(argv의 끝인 NULL)이다.

### initd(), process_create_initd()

여기서의 d는 OS를 보조하기위한 **daemon process**을 의미한다.

즉, 사용자의 프로그램이 실행되기 전에 먼저 실행되는 프로세스이다.

`process_create_initd()`의 설명을 보면 `initd()`라는 userland program을 실행한다고 써있는데 이는 `initd()`가 사용자영역에서 실행되므로 userland program이라고 표현한것이라 생각한다,..

### 📝 구현 함수

- create

  - [x] `push_argument_stack()`
    - [x] stack에 argument를 넣는다 [argment 컨벤션](https://casys-kaist.github.io/pintos-kaist/project2/argument_passing.html)
  - [x] `parse_argument()`
    - [x] argument를 parsing한다

- modify

  - [x] `process_exec()`
  - [x] `load()`

### 🎉 성공화면

#### 실행방법

```bash
pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'
```

#### 결과

```bash
root@f6123af02b64:/pintos-kaist/userprog/build# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'
qemu-system-x86_64: warning: TCG doesn't support requested feature: CPUID.01H:ECX.vmx [bit 5]
Kernel command line: -q -f put args-single run 'args-single onearg'
0 ~ 9fc00 1
100000 ~ ffe0000 1
Pintos booting with:
        base_mem: 0x0 ~ 0x9fc00 (Usable: 639 kB)
        ext_mem: 0x100000 ~ 0xffe0000 (Usable: 260,992 kB)
Calibrating timer...  52,377,600 loops/s.
hd0:0: detected 305 sector (152 kB) disk, model "QEMU HARDDISK", serial "QM00001"
hd0:1: detected 20,160 sector (9 MB) disk, model "QEMU HARDDISK", serial "QM00002"
hd1:0: detected 102 sector (51 kB) disk, model "QEMU HARDDISK", serial "QM00003"
Formatting file system...done.
Boot complete.
Putting 'args-single' into the file system...
Executing 'args-single onearg':
000000004747ffc0                          00 00 00 00 00 00 00 00 |        ........|
000000004747ffd0  ed ff 47 47 00 00 00 00-f9 ff 47 47 00 00 00 00 |..GG......GG....|
000000004747ffe0  00 00 00 00 00 00 00 00-00 00 00 00 00 61 72 67 |.............arg|
000000004747fff0  73 2d 73 69 6e 67 6c 65-00 6f 6e 65 61 72 67 00 |s-single.onearg.|
system call!
```

## PROJECT 2-2 system call

### 개념정리

- system call은 커널모드에서 실행하고 사용자모드로 return 한다

- `process`는 사실 `thread`이다. pintos에서 확인할 수 있는 가장 쉬운 방법은 `process_create_initd()`를 호출할때도 내부적으로 `thread_create(initd)`를 전달해주기도 하기 떄문이다.

### 구현 Tip

- `syscall-nr.h`에 시스템콜 번호가 적혀있다.

- `%rax`에 시스템 콜 번호가 넘어온다.

- `lib/user/syscall.c` 함수들은 user program에서만 호출 가능하다.

-

### 📝 구현 및 수정 함수

#### file manipulation

- `thread_create()` 내부

  - [x] file descriptor table 초기화
  - [x] **initailize** pointer to file descriptor table
  - [x] **reserve** fd0, fd1 for STDIN, STDOUT

- `process_exit()` 내부 (when terminated thread(process 종료))

  - [x] close all files
  - [x] free file descriptor table

- `file system call` 이 호출될때 **race condition**을 방지하기 위해 `global lock`을 사용한다.

  - [x] `syscall.h`에 `struct lock filesys_lock` 추가
  - [x] `syscall_init()`에서 `lock_init(&filesys_lock)` 추가

- ⭐️⭐️ test를 돌리려면 `process_wait`부터 구현해야한다.

  - [ ] `process_wait()`

- 어떤 시스템콜인지 확인하는 함수

  - [ ] `syscall_handler()`

- 해당 시스템콜에 해당하는 핸들러 구현

  - process
    - [x] `halt()`
    - [ ] `exit()`
    - [ ] `exec()`
    - [ ] `wait()`
  - file
    - [x] `create()`
    - [x] `remove()`
    - [x] `open()`
    - [x] `filesize()`
    - [x] `read()`
    - [x] `write()`
    - [ ] `seek()`
    - [ ] `tell()`
    - [x] `close()`

- 상위 **프로세스**(parent)와 하위 **프로세스**(child)의 **계층적** 구조 (hierarchy)

  - [ ] `struct thread` 멤버변수 추가
    - [ ] pointer to parent `struct thread* `
    - [ ] pointer to sibling `struct list`
    - [ ] pointer to child `struct list_elem`

- `semaphore`를 사용한 프로세스간의 동기화
  - [ ] `struct thread` 멤버변수 추가
    - [ ] `exit_status` : 프로세스의 종료상태를 저장하는 변수

### one more thing

- linux의 `exec`은 unix의 `exec`와 다르다. linux의 `exec`는 `execv`와 같은 기능을 한다.

### 사용할 함수

```c
/* Powers down the machine we're running on,
   as long as we're running on Bochs or QEMU. */
void
power_off (void) {
#ifdef FILESYS
	filesys_done ();
#endif

	print_stats ();

	printf ("Powering off...\n");
	outw (0x604, 0x2000);               /* Poweroff command for qemu */
	for (;;);
}
```

### 트러블슈팅

#### fd table 할당시 메모리 동적 할당 방식 고민

fd table을 정적할당 하려고 했다. 하지만 정적할당한 페이지는 프로그램 시작부터 끝까지 진행될 터이고프로세스가 종료되면 수거할 수 있는 동적할당이 낫다고 판단했다.

`malloc`을 사용해도 가능했고` `palloc`을 사용해도 가능했지만 OS 특성상 page 단위로 메모리를 관리하기에 `palloc`을 사용했다.

#### fdt를 초기화할때 어디서 할 것인가 ?

`thread_init()`에서 해주려 했지만 생각해보니 `init()`이란건 `thread`가 언제든 초기화할때 사용할 수 있는 함수라 생각했다

그렇다면 fdt는 `thread_init()`을 실행할따마다 할당하게되고 메모리 누수가 발생할것이다.

그렇다면 생성시점에 단 한번만 실행되는 함수를 찾아야했다

그렇게해서 찾은것이 `thread_create()` 이다.

`thread_create()`는 쓰레드가 생성할때 단 한번만 실행되므로

#### fdt[0]과 fdt[1]은 어째서 0과 1을 넣어주는것일까

이는 단순히 **dummy** 값이다. 다른 fd와 달리 file을 사용하는게 아니라 `input_getc()`와 `putbuf()`를 사용하기에 의미없는 `file`인 0과 1을 넣어준것이다.

#### fork에 if를 넘겨줄떄 struct fork_frame 이라는 구조체를 만들어 넘기려했다.

```c
struct fork_frame {
  struct intr_frame if_;
  struct thread *parent;
};
```

### 커널

커널이란?

커널은 프로세스가 아닌 **운영체제**의 서비스를 제공하는 함수, 데이터의 집합체다.

커널영역 이라는 단어떄문에 커널이 하나의 프로세스 처럼 보이지만 커널은 프로세스가 아니다.

커널영역이라는것은 커널이 제공할 서비스를 메모리에 적재하고있고 해당 주소(메모리블럭)집합을 커널영역이라 부른다.

커널영역은 시스템콜이나 인터럽트를 처리할때만 접근할 수 있다.

커널은 프로세스가 아니기에 스택과 힙 영역이 존재하지 않지만 커널쓰레드로써 작업을 수행할 수 있다.

즉, 프로세스는 아니지만 커널 쓰레드라는 형태로 실행되기에 스택과 힙을 사용할 수 있다.

### read(), write 시스템콜의 위대함

`fd == 0` 일때 stdin을 읽어오는것이다.

입력이란 키보드로부터 들어오는것이다. `input_getc`를 통해 키보드의 입력을 한글자 한글자 받아오고 입력이 완료될떄까지 기다린다.

다시말해, 우리가 지금까지 게임, 타자연습, vscode, 터미널 등등을 사용할때 키보드로 입력을 받아오는것이 `read()` 시스템콜이었다. ㅁㅊㄷ

또한 입력들이 화면이나 터미널, vscode로 출력되는것이 `write()` 시스템콜을 호출했을때 `fd == 1`이다. ㅁㅊㄷ

#### 테스트 실행방법

```bash
pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/create-null:create-null -- -q   -f run create-null
```

```bash
make tests/userprog/close-bad-fd.result VERBOSE=1
```

# PROJECT 3

## memory management & lazy loading & anonymous page

### 개념

- **page**

  - page란 메모리 가상메모리의 블럭 단위이다. page는 4KB로 고정되어있다.

- **frame**

  - frame은 물리메모리의 블럭 단위이다. frame은 4KB로 고정되어있다.

### 현재 pintos의 문제

#### 현재 pintos 메모리 관리 상황

code segment와 Data 즉, file을 읽어와 `setup_stack()`을 통해 물리메모리를 할당하고 데이터를 load한다.

#### vm 이후의 pintos

허나 이제는 page_table을 미리 만들어놓고 필요할때만 물리메모리를 할당받아 데이터를 load하고 page table을 셋팅하는 방식으로 바꿔야한다.

다시말해, page_table에 page만을 할당하고 물리메모리에는 할당하지 않은 상태이다.

### 💡 발견

### 구현목록

- For virtual memory

  - [x] `vm_{func}` 관련 함수 구현
  - [x] `page_table` 구현

- For lazy loading(demanding page)

  - [x] `load_segment()` 수정
  - [x] `page_fault_handler()` 수정
  - [x] `lazy_load_segment` 수정

- For anonymous page

  - supplementary_page_table
    - [x] `supplemental_page_table_copy()` 구현
    - [x] `supplemental_page_table_kill()` 구현
  - anon.c
    - [x] `anon_destroy()` 구현
  - uninit.c
    - [x] `uninit_destroy()` 구현

- validation
  - 수정목록
    - [x] `check_address()` 수정
  - 구현목록
    - [x] `check_valid_buffer()` 구현
    - [ ] `check_valid_string()` 구현

### 트러블슈팅

`page_get_type()` 의 경우 `uninit_page`일때는 후추 claim될때 해당 파일의 타입으로 변경될 타입을 반환한다.

즉, `VM_UNINIT`을 반환하는게 아니라 `VM_FILE`이나 `VM_ANON`을 반환한다.

그렇기에 `page_get_type()`을 사용해 `VM_UNINIT` 타입이 `VM_ANON`으로 반환됐을때

frame이 할당이 안됐기에 claim해줘야 했는데 실제 page type인 operation 타입을 확인하면 상관이 없어진다.

```c
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
                                  struct supplemental_page_table *src UNUSED) {
  struct hash_iterator iterator;
  struct page *parent_page, *child_page = NULL;
  struct file_segment_info *src_aux, *dst_aux = NULL;
  enum vm_type curr_page_type;
  bool succ = false;

  hash_first(&iterator, &src->page_table);

  while (hash_next(&iterator)) {
    parent_page = hash_entry(hash_cur(&iterator), struct page, spt_elem);
    curr_page_type = page_get_type(parent_page);

    switch (curr_page_type) {

      case VM_UNINIT:
        src_aux = (struct file_segment_info *)parent_page->uninit.aux;
        dst_aux = (struct file_segment_info *)calloc(
            1, sizeof(struct file_segment_info));
        if (!dst_aux) goto err;

        memcpy(dst_aux, src_aux, sizeof(struct file_segment_info));
        dst_aux->file = file_duplicate(src_aux->file);
        /* memcpy 오류시 다음과 같이 변경될 수 있음
           dst_aux->read_bytes = src_aux->read_bytes;
           dst_aux->offset = src_aux->offset;
           dst_aux->zero_bytes = src_aux->zero_bytes; */

        if (!vm_alloc_page_with_initializer(
                page_get_type(parent_page), parent_page->va,
                parent_page->writable, parent_page->uninit.init, dst_aux)) {

          free(dst_aux);
          goto err;
        }
        break;

      case VM_ANON:
        if (!vm_alloc_page(VM_ANON | VM_MARKER_0, parent_page->va,
                           parent_page->writable)) {
          goto err;
        }
        if (!vm_claim_page(parent_page->va)) goto err;

        child_page = spt_find_page(dst, parent_page->va);
        if (!child_page) goto err;

        if (parent_page->frame == NULL) {
          vm_do_claim_page(parent_page);
        }

        memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);

        break;
        /* TODO : copy-on-write 구현한다면 부모의 kva를 자식의 va가 가르키도록 설정 */
    }
  }

  succ = true;

err:

  return succ;
}
```

## stack growth

### 개념

user stack은 user program이 실행될때 생성되는 stack이다.

user program이 실행되면서 지역변수들을 stack에 넣게되는데

이때 push 인스트럭션을 통해 user stack에 넣게된다.

하지만 기본 할당받은 4kb의 user stack을 넘게되면 stack을 expanding 해줘야하는데

이것이 stack growth이다.

### 구현목록

- [x] `stack_growth()` 구현
- [x] `vm_try_handle_fault()` 수정

### 트러블슈팅

#### load_segment()에서 while문에 file의 va가 400000으로 5개 시작하는 이유

load_segment를 했을떄 조각냈을떄 400000 으로 시작됐떤건 code 영역이기에 그렇다또한 args-none이 5page가 필요하기에 40000부터 40001000까지 할당되는데 1000차이가 나는건 16진수로 4kb 차이가 나기에 1page 차이가 나는것이다.

> 1000이라는 16진수가 10진수로 4096인 이유 2^4 \* 16 = 4096

4000000 4001000 4002000 4003000 ...

뒤의 자리가 000인 이유는 offset이기 때문이다.

code에 args.noen을 5page 짤라넣는것

#### stack growth에서 rsp는 한번에 확 밀리는가

함수의 호출이 일어날경우 rsp는 한번에 밀릴지 8바이트씩 밀릴지 궁금했다.

test case중에 `pt-big-stk-obj`라는 테스트를 확인하면 65536바이트의 지역변수가 선언되어 있다.

이를 확인해보니 8바이트씩 밀리면서 지역변수의 rsp를 밀어주는것이 아닌 65536바이트를 한번에 밀어주고 stack의 위로 올라가면서

page를 할당하고 있다.

ps. 이부분은 `pt-big-stk-obj` 테스트 뿐만 아니라 일반적인 stack-growth 테스트를 같이 확인해야 알 수 있는데

`pt-big-stk-obj` 의 rsp는 1195835008으로 시작하고 `pt-grow-stack`의 rsp는 1195896448로 시작한다.

즉, `pt-grow-stack`의 rsp - `pt-big-stk-obj`의 rsp = 61440 이다.

시작점이 지역변수인 `stk_obj` char 배열의 크기만큼인것을 확인할 수 있다

그렇다면 우리는 한가지 의문이 든다.

rsp를 많이 밀어주었을때 while문을 돌면서 page를 할당해주는데

while문의 종료구문인 `spt_find_page()`에 의해 종료가된다.

이말인 즉슨, vitual page를 4kb씩 밀어주면서 할당하다가 이미 존재하는 page가 있을때 종료가 된다는것인데

그렇다면 다른 virtual page를 만났을때 우리가 필요한 만큼의 stack growth가 됐다는걸 어떻게 보장한다는 것일까 ?

결론부터 말하면 우리는 이런 case를 처리하기위해 지속적으로 한가지 규칙이 있었다

그것중 하나는 4kb 1page당 4096바이트를 정렬시켜준 이유가 여기 있다.

또한 rsp-8이 라는것은 1개의 **push** 인스트럭션을 위한 단위이기에 스택에 넣기위해 push를 할때 page fault가 발생하는것이다.

ps. 여담이지만 pintos-kaist youtube 강의를 보면 rsp-32 즉, 32byte를 밀어주고 확인하는데

이는 PUSHA 라는 인스트럭션이 32비트 운영체제에서는 존재하기 떄문이다

PUSHA 명령어는 현제 레지스터에 있는 모든 값을 스택에 저장해두는데 (이를 호출하는 경우는 컨텍스트 스위치가 일어났을때 현재 레지스터의 모든 상태값을 저장해야할때)

32비트 운영체제의 레지스터는 8개가 있고 1word는 4바이트 이므로 4 \* 8 = 32바이트가 된다.

그렇다면 64비트 운영체제에서는 16개의 레지스터가 있기에 16 \* 8(1word == 8byte) = 128바이트가 되어야 하는데 어째서 8byte의 push만 확인할까 ?

이는 64비트 운영체제에서는 PUSHA가 없어지고 필요한 레지스터만 push할 수 있도록 변경되었기에 rsp-8로 확인하는것이다.

```
------------------------------------------------
|                pt-big-stk-obj                |
------------------------------------------------

  (pt-big-stk-obj) begin
+ ----- CATCH handle_fault !! -----
+ addr : 1195835000
+ f->rsp : 1195835008
+ USER_STACK : 1195900928
+ USER_STACK - STACK_LIMIT : 1194852352
+ --------------------------------
+
+ ------stack growth() START !-------
+ @param addr : 1195835000
+ @var pg_round_down(addr) : 1195831296
+  page_addr + PGSIZE : 1195835392
+  page_addr + PGSIZE : 1195839488
+  page_addr + PGSIZE : 1195843584
+  page_addr + PGSIZE : 1195847680
+  page_addr + PGSIZE : 1195851776
+  page_addr + PGSIZE : 1195855872
+  page_addr + PGSIZE : 1195859968
+  page_addr + PGSIZE : 1195864064
+  page_addr + PGSIZE : 1195868160
+  page_addr + PGSIZE : 1195872256
+  page_addr + PGSIZE : 1195876352
+  page_addr + PGSIZE : 1195880448
+  page_addr + PGSIZE : 1195884544
+  page_addr + PGSIZE : 1195888640
+  page_addr + PGSIZE : 1195892736
+  page_addr + PGSIZE : 1195896832
+ ------stack growth() DONE !-------
+
  (pt-big-stk-obj) cksum: 3256410166
  (pt-big-stk-obj) end

------------------------------------------------
|                pt-grow-stack                 |
------------------------------------------------
  Acceptable output:
  (pt-grow-stack) begin
  (pt-grow-stack) cksum: 3424492700
  (pt-grow-stack) end
Differences in `diff -u' format:
  (pt-grow-stack) begin
+ ----- CATCH handle_fault !! -----
+ addr : 1195896440
+ f->rsp : 1195896448
+ USER_STACK : 1195900928
+ USER_STACK - STACK_LIMIT : 1194852352
+ --------------------------------
+
+ ------stack growth() START !-------
+ @param addr : 1195896440
+ @var pg_round_down(addr) : 1195892736
+  page_addr + PGSIZE : 1195896832
+ ------stack growth() DONE !-------
+
  (pt-grow-stack) cksum: 3424492700
  (pt-grow-stack) end
```
