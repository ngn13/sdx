#include "sched/sched.h"
#include "util/lock.h"

void spinlock_acquire(spinlock_t *lock) {
  while (spinlock_locked(lock))
    sched_wait();
  __asm__("lock bts $0, (%0)" ::"r"(lock));
}
