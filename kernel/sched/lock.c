#include "sched/sched.h"
#include "sched/task.h"
#include "util/lock.h"

#include "errno.h"
#include "types.h"

int32_t task_lock_add(task_t *task, spinlock_t *lock) {
  if (NULL == task || NULL == lock)
    return -EINVAL;

  // check the current task lock depth
  if (task->lock_depth >= sizeof(task->locks))
    return -EFAULT;

  // set the current lock we are waiting on
  task->lock = lock;
  return 0;
}

int32_t task_lock_push(task_t *task) {
  if (NULL == task || NULL == task->lock)
    return -EINVAL;

  // add the lock to the list
  task->locks[task->lock_depth++] = task->lock;

  // clear the current lock we are waiting on
  task->lock = NULL;
  return 0;
}

spinlock_t *task_lock_pop(task_t *task) {
  if (NULL == task || task->lock_depth == 0)
    return NULL;

  // remove (pop) the last lock
  spinlock_t *lock              = task->locks[--task->lock_depth];
  task->locks[task->lock_depth] = NULL;

  // return the lock
  return lock;
}
