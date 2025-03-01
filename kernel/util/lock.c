#include "sched/sched.h"
#include "util/lock.h"

void spinlock_acquire(spinlock_t *lock) {
  while (*lock & 1) {
    sched_state(TASK_STATE_WAIT);
    sched();
  }
  __asm__("lock bts $0, (%0)" ::"r"(lock));
}
