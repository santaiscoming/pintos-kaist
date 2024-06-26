#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops(unsigned loops);
static void busy_wait(int64_t loops);
static void real_time_sleep(int64_t num, int32_t denom);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
void timer_init(void) {
  /* 8254 input frequency divided by TIMER_FREQ, rounded to
     nearest. */
  uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

  outb(0x43, 0x34); /* CW: counter 0, LSB then MSB, mode 2, binary. */
  outb(0x40, count & 0xff);
  outb(0x40, count >> 8);

  intr_register_ext(0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void timer_calibrate(void) {
  unsigned high_bit, test_bit;

  ASSERT(intr_get_level() == INTR_ON);
  printf("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops(loops_per_tick << 1)) {
    loops_per_tick <<= 1;
    ASSERT(loops_per_tick != 0);
  }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops(high_bit | test_bit)) loops_per_tick |= test_bit;

  printf("%'" PRIu64 " loops/s.\n", (uint64_t)loops_per_tick * TIMER_FREQ);
}

/**
 * @brief Returns the number of timer ticks since the OS booted.
 *
 * @return 부팅 이후 해당 시점까지의 tick 수 반환
 *
 * @details :
 *         ticks를 읽는동안 인터럽트가 발생하지 않도록 인터럽트를 막는다
 *         이후 t에 tick 저장하고 위에서 disable한 인터럽트를 다시 활성화한다
 *         이때 barrier()를 사용하여 컴파일러 최적화에 의해 명령어 접근 순서가
 *         변경되는것을 막는다 즉, 메모리 작업이 예정된 수순으로 다시 돌아가길
 *         원한다.
 */
int64_t timer_ticks(void) {
  enum intr_level old_level = intr_disable();
  int64_t t = ticks;
  intr_set_level(old_level);
  barrier();
  return t;
}

/**
 * @brief 특정 시점 이후의 경과된 tick 수 반환
 * 
 * @param then 특정 시점
 * 
 * @example timer_sleep() 함수에서 start에 현재 tick을 저장하고
 *          while문으로 timer_elapsed(start) < ticks로 loop를
 *          돌게한다.
 *          예를들어, start에 100을 저장하고 200까지 sleep하라고 가정하자
 *          while문을 돌면서 timner_elapsed(start)는 100, 101, 102, 
 *          103, 104, ... 199, 200까지 증가하게 된다. 
 *          d이후 200(ticks)가 되면 loop를 빠져나오게 된다.
 *          ⛔️ start로 선언한 지역변수는 변하지않는다.
 * 
 * @note Returns the number of timer ticks elapsed since THEN, which
 *       should be a value once returned by timer_ticks().
*/
int64_t timer_elapsed(int64_t then) { return timer_ticks() - then; }

/**
 * @brief ticks(ms)만큼 thread를 잠재운다.
 *
 * @param ticks 잠재울 시간
 *
 * @note Suspends execution for approximately TICKS timer ticks.
 */
void timer_sleep(int64_t ticks) {
  /*
    - tick : process or thread의 cpu 점유 시간
    - timer_elapsed(start) : 현재 tick - 이전 tick
  */
  int64_t start = timer_ticks(); /* 부팅된 이후부터 현재까지의 경과 시간 반환 */
  ASSERT(intr_get_level() == INTR_ON); /* 인터럽트가 활성화되어 있는지 확인 */
  /* before Project.1 */
  // while (timer_elapsed(start) < ticks) thread_yield();

  /* ------------ added for Project.1 ------------ */
  thread_sleep(start + ticks);
  /* -------------------- end -------------------- */
}

/* Suspends execution for approximately MS milliseconds. */
void timer_msleep(int64_t ms) { real_time_sleep(ms, 1000); }

/* Suspends execution for approximately US microseconds. */
void timer_usleep(int64_t us) { real_time_sleep(us, 1000 * 1000); }

/* Suspends execution for approximately NS nanoseconds. */
void timer_nsleep(int64_t ns) { real_time_sleep(ns, 1000 * 1000 * 1000); }

/* Prints timer statistics. */
void timer_print_stats(void) {
  printf("Timer: %" PRId64 " ticks\n", timer_ticks());
}

/**
 * @brief timer interrupt 발생시 실행할 함수
 *
 * @note Timer interrupt handler.
 */
static void timer_interrupt(struct intr_frame *args UNUSED) {
  ticks++;
  thread_tick();

  /* ---------- added for Project.1-1 ---------- */

  thread_check_awake(ticks);

  /* ---------- added for Project.1-3 ---------- */

  if (!thread_mlfqs) return;

  /* each ticks excute */
  thread_increase_recent_cpu_of_running();

  /* every 1 second */
  if (ticks % TIMER_FREQ == 0) {
    thread_update_load_avg();
    thread_foreach(thread_update_recent_cpu, NULL);
  }

  /* every 4 ticks */
  if (ticks % 4 == 0) {
    thread_foreach(thread_set_priority_mlfqs, NULL);
  }

  /*------------------------------------------*/
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool too_many_loops(unsigned loops) {
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start) barrier();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait(loops);

  /* If the tick count changed, we iterated too long. */
  barrier();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE busy_wait(int64_t loops) {
  while (loops-- > 0) barrier();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void real_time_sleep(int64_t num, int32_t denom) {
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.

     (NUM / DENOM) s
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
     1 s / TIMER_FREQ ticks
     */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT(intr_get_level() == INTR_ON);
  if (ticks > 0) {
    /* We're waiting for at least one full timer tick.  Use
       timer_sleep() because it will yield the CPU to other
       processes. */
    timer_sleep(ticks);
  } else {
    /* Otherwise, use a busy-wait loop for more accurate
       sub-tick timing.  We scale the numerator and denominator
       down by 1000 to avoid the possibility of overflow. */
    ASSERT(denom % 1000 == 0);
    busy_wait(loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
  }
}
