/* Measures the correctness of the "nice" implementation.

   The "fair" tests run either 2 or 20 threads all niced to 0.
   The threads should all receive approximately the same number
   of ticks.  Each test runs for 30 seconds, so the ticks should
   also sum to approximately 30 * 100 == 3000 ticks.

   The mlfqs-nice-2 test runs 2 threads, one with nice 0, the
   other with nice 5, which should receive 1,904 and 1,096 ticks,
   respectively, over 30 seconds.

   The mlfqs-nice-10 test runs 10 threads with nice 0 through 9.
   They should receive 672, 588, 492, 408, 316, 232, 152, 92, 40,
   and 8 ticks, respectively, over 30 seconds.

   (The above are computed via simulation in mlfqs.pm.)

   "nice" 구현의 정확성을 측정합니다.

   "fair" 테스트는 2개 또는 20개의 스레드를 모두 0으로 설정하여 실행합니다.
   스레드는 모두 거의 동일한 수의 틱을 수신해야 합니다.
   각 테스트는 30초 동안 실행되므로 틱의 합은 약 30 * 100 == 3000 틱이
   되어야 합니다.

   mlfqs-nice-2 테스트는 2개의 스레드를 실행하는데, 하나는 nice 0이고,
   다른 하나는 nice 5이며, 각각 1,904개와 1,096개의 틱을 30초 이상 수신해야 한다.

   mlfqs-nice-10 테스트는 nice 0부터 9까지 10개의 스레드를 실행합니다.
   각각 672, 588, 492, 408, 316, 232, 152, 92, 40, 8틱을 30초 이상
   받아야 한다.

   (위의 내용은 mlfqs.pm 의 시뮬레이션을 통해 계산됩니다.) */

#include <inttypes.h>
#include <stdio.h>
#include "devices/timer.h"
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

static void test_mlfqs_fair(int thread_cnt, int nice_min, int nice_step);

void test_mlfqs_fair_2(void) { test_mlfqs_fair(2, 0, 0); }

void test_mlfqs_fair_20(void) { test_mlfqs_fair(20, 0, 0); }

void test_mlfqs_nice_2(void) { test_mlfqs_fair(2, 0, 5); }

void test_mlfqs_nice_10(void) { test_mlfqs_fair(10, 0, 1); }

#define MAX_THREAD_CNT 20

struct thread_info {
  int64_t start_time;
  int tick_count;
  int nice;
};

static void load_thread(void *aux);

static void test_mlfqs_fair(int thread_cnt, int nice_min, int nice_step) {
  struct thread_info info[MAX_THREAD_CNT];
  int64_t start_time;
  int nice;
  int i;

  ASSERT(thread_mlfqs);
  ASSERT(thread_cnt <= MAX_THREAD_CNT);
  ASSERT(nice_min >= -10);
  ASSERT(nice_step >= 0);
  ASSERT(nice_min + nice_step * (thread_cnt - 1) <= 20);

  thread_set_nice(-20);

  start_time = timer_ticks();
  msg("Starting %d threads...", thread_cnt);
  nice = nice_min;
  for (i = 0; i < thread_cnt; i++) {
    struct thread_info *ti = &info[i];
    char name[16];

    ti->start_time = start_time;
    ti->tick_count = 0;
    ti->nice = nice;

    snprintf(name, sizeof name, "load %d", i);
    thread_create(name, PRI_DEFAULT, load_thread, ti);

    nice += nice_step;
  }
  msg("Starting threads took %" PRId64 " ticks.", timer_elapsed(start_time));

  msg("Sleeping 40 seconds to let threads run, please wait...");
  timer_sleep(40 * TIMER_FREQ);

  for (i = 0; i < thread_cnt; i++)
    msg("Thread %d received %d ticks.", i, info[i].tick_count);
}

static void load_thread(void *ti_) {
  struct thread_info *ti = ti_;
  int64_t sleep_time = 5 * TIMER_FREQ;
  int64_t spin_time = sleep_time + 30 * TIMER_FREQ;
  int64_t last_time = 0;

  thread_set_nice(ti->nice);
  timer_sleep(sleep_time - timer_elapsed(ti->start_time));
  while (timer_elapsed(ti->start_time) < spin_time) {
    int64_t cur_time = timer_ticks();
    if (cur_time != last_time) ti->tick_count++;
    last_time = cur_time;
  }
}
