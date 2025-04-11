#include "sched/sched.h"
#include "sched/task.h"

#include "types.h"
#include "errno.h"

pid_t sys_wait(int32_t *status) {
  task_waitq_t *waitq = NULL;
  pid_t         pid   = 0;

  // if the wait queue is not empty, just use the next waitq
  if (!task_waitq_is_empty(current))
    goto end;

  /*

   * check if we have any children before waiting on a wait
   * queue update, since if we don't have any children, task's
   * wait queue will never be updated

  */
  if (NULL == sched_child(current, NULL))
    return -ECHILD;

  // wait for a waitq
  sched_block_until(TASK_BLOCK_WAIT, task_waitq_is_empty(current));

end:
  // get the current waitq in the queue
  waitq = task_waitq_pop(current);

  // get the waitq status and PID
  *status = waitq->status;
  pid     = waitq->pid;

  // free the waitq
  task_waitq_free(waitq);

  // return the PID
  return pid;
}
