# Project 1

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

### nice

### To-do

#### props

- [x] `nice`,`recent_cpu` property 추가

  - [x] `nice`,`recent_cpu` default 및 범위 추가

-[x] `thread_init` 수정

-[x] `thread`생성시에 `priority` **31** 로 초기화

#### function

- calc
  - [ ] `calc_priority()` using `calc_recent_cpu()`, `calc_nice()`
  - [ ] `calc_recent_cpu()`
  - [ ] `load_avg()`
  - [ ] `increase_recent_cpu()` 1씩 증가
  - [ ] 모든 쓰레드에 대해 `re_calc_priority()`, `re_calc_recent_cpu()`

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
