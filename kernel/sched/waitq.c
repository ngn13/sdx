#include "sched/sched.h"
#include "sched/task.h"
#include "mm/heap.h"

#include "util/mem.h"
#include "util/list.h"

#include "errno.h"
#include "types.h"

int32_t task_waitq_add(task_t *task, task_t *child) {
  // check the arguments
  if (NULL == task || NULL == child)
    return -EINVAL;

  task_waitq_t *waitq = heap_alloc(sizeof(task_waitq_t));

  // check if failed to allocate the waitq object
  if (NULL == waitq)
    return -ENOMEM;

  // setup the basic waitq info
  bzero(waitq, sizeof(task_waitq_t));
  waitq->pid = child->pid;

  // setup the status
  waitq->status |= (int32_t)child->exit_code << 8;
  waitq->status |= (int32_t)child->term_code & 0xffff;

  // add new waitq to the task wait queue
  if (NULL == task->waitq_head || NULL == task->waitq_tail)
    task->waitq_head = task->waitq_tail = waitq;
  else
    task->waitq_tail->next = waitq;

  // unblock the task (if it's blocked)
  sched_unblock(task, TASK_BLOCK_WAIT);

  return 0;
}

task_waitq_t *task_waitq_pop(task_t *task) {
  // check the arguments
  if (NULL == task)
    return NULL;

  // return the first waitq in the queue
  task_waitq_t *waitq = task->waitq_head;

  // check if there is anything in the wait queue
  if (NULL == waitq)
    return NULL;

  // remove the first waitq from the queue
  slist_del(&task->waitq_head, waitq, task_waitq_t);

  // remove the possible tail reference
  if (waitq == task->waitq_tail)
    task->waitq_tail = waitq->next;

  return waitq;
}
