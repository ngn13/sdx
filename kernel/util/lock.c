#include "sched/sched.h"
#include "sched/task.h"
#include "util/lock.h"

void spinlock_release() {
  // get the last lock the task acquired
  spinlock_t *lock = task_lock_pop(current);

  if (NULL == lock)
    return;

  // unlock the lock
  *lock = 0;

  // loop through all the tasks
  sched_foreach() {
    // check if the task is waiting on this lock
    if (cur->lock != lock)
      continue;

    // if so, unblock the task (if its blocked)
    if (sched_unblock(cur, TASK_BLOCK_LOCK) == 0)
      return;
  }
}

void spinlock_acquire(spinlock_t *lock) {
  /*

   * save the lock so it will be added to the task's list when we
   * call task_lock_push() for the same task next

  */
  if (task_lock_add(current, lock) != 0)
    return;

  // block the task until the lock is available
  sched_block_until(TASK_BLOCK_LOCK, spinlock_locked(lock));

  // push the lock to the list
  task_lock_push(current);

  // acquire the lock
  __asm__("lock bts $0, (%0)" ::"r"(lock));
}
