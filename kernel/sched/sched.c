#include "sched/sched.h"
#include "sched/task.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/panic.h"
#include "util/list.h"
#include "util/bit.h"

#include "core/im.h"
#include "core/pic.h"

#include "mm/vmm.h"
#include "mm/heap.h"

#include "errno.h"
#include "types.h"

// used to keep track of the task queue
task_t *task_head = NULL, *task_tail = NULL;
task_t *task_current  = NULL; // current running task
task_t *task_promoted = NULL; // promoted task will always run next

#define __sched_print_task(task)                                                                                       \
  do {                                                                                                                 \
    sched_debg("|- Name : %s", task->name);                                                                            \
    sched_debg("|- PID  : %d", task->pid);                                                                             \
    sched_debg("|- RIP  : 0x%x", task->regs.rip);                                                                      \
    sched_debg("`- Stack: 0x%x", task->regs.rsp);                                                                      \
  } while (0)

// delete a task from the task queue
#define __sched_queue_del(task) dlist_del(&task_head, &task_tail, task, task_t)

// add task to scheduler queue (higher priority task is placed before lower ones)
int32_t __sched_queue_add(task_t *task) {
  if (NULL == task_head && NULL == task_tail) {
    task_head = task_tail = task;
    task->next            = NULL;
    task->prev            = NULL;
    return 0;
  }

  dlist_reveach(&task_tail, task_t) {
    /*

     * our priority should be lower or equal to the current task
     * so we'll be able to place it after it without breaking the
     * order in the queue

    */
    if (task->prio > cur->prio)
      continue;

    // place the task after the current (cur) task
    task->prev = cur;
    task->next = cur->next;
    cur->next  = task;

    if (NULL != task->next)
      task->next->prev = task;
  }

  /*

   * if the task have a higher priority then the current task (task_current)
   * we should promote the new task so we'll switch to it next time scheduler
   * timer is called

  */
  if (task->prio > task_current->prio)
    task_promoted = task;

  return 0;
}

void __sched_queue_clean() {
  task_t *corpse = NULL;

  // look for the dead task
  dlist_foreach(&task_head, task_t) {
    if (cur->state == TASK_STATE_DEAD) {
      corpse = cur;
      break;
    }
  }

  // we should have switch to a new task
  if (NULL == corpse || task_current == corpse)
    return;

  // remove from the list
  __sched_queue_del(corpse);

  // dismember
  task_free(corpse);
}

// get next task in the queue
task_t *__sched_queue_next() {
  task_t *pos = task_current;

  do {
    // promoted task will always be selected next
    if (NULL != task_promoted) {
      pos           = task_promoted;
      task_promoted = NULL;
      continue;
    }

    // if we reached the end of the queue, go back to the start
    if (NULL == pos || NULL == pos->next) {
      pos = task_head;
      continue;
    }

    // get the next task using the next pointer
    pos = pos->next;
  } while (pos->state == TASK_STATE_DEAD);

  return pos;
}

// find the next available PID
void __sched_pid(task_t *task) {
  task->pid = 0;

  dlist_reveach(&task_tail, task_t) {
    if (cur->pid > task->pid)
      task->pid = cur->pid;
  }

  if (task->pid == PID_MAX)
    panic("Reached the max PID");

  task->pid++;
}

// scheduler timer interrupt handler
void __sched_timer_handler(im_stack_t *stack) {
  task_t *task_new = NULL;

  /*

   * if we don't have a task selected (so first time running)
   * get the next task in the queue, which would be the first
   * and the current task

  */
  if (NULL == task_current)
    task_current = __sched_queue_next();

  // if we received a signal, handle it
  if (!task_sigset_empty(task_current))
    task_signal_pop(task_current);

  // handle the state of the current task
  switch (task_current->state) {
  case TASK_STATE_READY:
    // update the registers of the current task
    task_update_regs(task_current, stack);
    break;

  case TASK_STATE_SAVE:
    /*

     * registers should be saved, don't update them
     * update the interrupt stack instead

    */
    task_update_stack(task_current, stack);
    break;

  case TASK_STATE_WAIT:
    /*

     * wait state means task is waiting on something
     * so we can skip to the next task, after saving
     * the registers so we'll continue where we left
     * of when we get to this task again

    */
    task_update_regs(task_current, stack);
    task_current->ticks = 0;
    break;

  case TASK_STATE_DEAD:
    /*

     * dead state means that task is no longer with us
     * send our prayers to the terry davis

     * task will be free'd and remove from the task list
     * after the next task switch with __sched_queue_clean

    */

    // TODO: notify any tasks waiting on this task

    task_current = NULL;
    break;

  case TASK_STATE_FORK:
    /*

     * fork state means we should fork the current task

     * to do so, we'll use task_copy() to copy all the memory regions
     * and the stuff, we'll create a new task and add it to the task
     * queue, we'Ll also copy the registers and stuff so the fork
     * will start running where the parent left of

    */
    task_update_regs(task_current, stack);

    sched_debg("forking the current task (PID %d)", task_current->pid);
    task_new = task_copy();

    if (NULL == task_new) {
      sched_fail("failed to copy the task 0x%p for forking", task_current);
      break;
    }

    // set required values
    __sched_pid(task_new);               // give a PID to the new task
    task_new->state = TASK_STATE_READY;  // default state
    task_new->prio  = TASK_PRIO_LOW;     // default priority
    task_new->ppid  = task_current->pid; // set the PPID

    // add new task to the task list
    sched_debg("forked the current task (PID %d -> %d)", task_current->pid, task_new->pid);
    __sched_queue_add(task_new);

    // update the last child PID of the parent
    task_current->cpid = task_new->pid;
    break;

  default:
    // if we get here, something is wrong
    sched_warn("task is in an unknown state, putting it back to ready state");
    task_current->state = TASK_STATE_READY;
    break;
  }

  /*

   * if the current task has no more remaining ticks
   * or if it doesn't exist anymore, switch to the next task

  */
  if (NULL == task_current || task_current->ticks <= 0) {
    // get the new task
    if ((task_new = __sched_queue_next()) != task_current)
      sched_debg("switching to the next task (PID: %d)", task_new->pid);

    // update the current task
    task_current = task_new;

    // switch to the new task
    task_ticks_reset(task_current);
    task_update_stack(task_current, stack);
    task_switch(task_current);

    // cleanup previous dead task
    __sched_queue_clean();
  }

  // reset the state of the task
  task_current->state = TASK_STATE_READY;

  /*

   * decrement the remaining ticks of the current task
   * before jumping back to it again

  */
  task_current->ticks--;
  return;
}

void __sched_exception_handler(im_stack_t *stack) {
  switch (stack->vector) {
  case IM_INT_DIV_ERR:
    sched_fail("#DE fault at 0x%x", stack->rip);
    task_signal_add(current, SIGSEGV);
    break;

  case IM_INT_INV_OPCODE:
    sched_fail("#UD fault at 0x%x", stack->rip);
    task_signal_add(current, SIGILL);
    break;

  case IM_INT_DOUBLE_FAULT:
    sched_fail("#DF abort at 0x%x", stack->rip);
    task_signal_add(current, SIGSEGV);
    break;

  case IM_INT_GENERAL_PROTECTION_FAULT:
    sched_fail("#GP fault at 0x%x", stack->rip);
    task_signal_add(current, SIGSEGV);
    break;

  case IM_INT_PAGE_FAULT:
    sched_fail("#PF fault at 0x%x", stack->rip);
    printf("            P=%u W=%u U=%u R=%u I=%u PK=%u SS=%u SGX=%u\n",
        bit_get(stack->error, 0),
        bit_get(stack->error, 1),
        bit_get(stack->error, 2),
        bit_get(stack->error, 3),
        bit_get(stack->error, 4),
        bit_get(stack->error, 5),
        bit_get(stack->error, 6),
        bit_get(stack->error, 7));
    task_signal_add(current, SIGSEGV);
    break;

  default:
    sched_fail("unknown fault (0x%x) at 0x%x", stack->vector, stack->rip);
    task_signal_add(current, SIGSEGV);
    break;
  }

  if (NULL == task_current)
    panic("Exception during scheduler initialization");
}

int32_t sched_init() {
  task_t *task_main = NULL;
  int32_t err       = 0;

  // clear the queue
  task_current = task_head = task_tail = NULL;

  // mask the timer interrupt during initialization of the scheduler
  pic_mask(PIC_IRQ_TIMER);

  // add the scheduler handler
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), IM_HANDLER_PRIO_SECOND, __sched_timer_handler);

  // add the exception handlers
  for (uint8_t i = 0; i < IM_INT_EXCEPTIONS; i++) {
    im_add_handler(i, IM_HANDLER_PRIO_SECOND, __sched_timer_handler);
    im_add_handler(i, IM_HANDLER_PRIO_SECOND, __sched_exception_handler);
  }

  // setup the default signal handlers
  if ((err = task_signal_setup()) != 0) {
    sched_fail("failed to setup task signal handlers: %s", strerror(err));
    return err;
  }

  // setup the current and the first task (pid 1)
  if ((task_main = task_new()) == NULL) {
    sched_debg("failed to setup the main task");
    return -EFAULT;
  }

  // rename the main task
  sched_info("created the main task: 0x%p", task_main);

  // set the required values
  __sched_pid(task_main);
  task_rename(task_main, "main");
  task_main->state = TASK_STATE_READY;
  task_main->prio  = TASK_PRIO_LOW;
  task_main->ppid  = 0;

  // add new task to the task list
  __sched_queue_add(task_main);

  // unmask the timer interrupt for the scheduler
  if (!pic_unmask(PIC_IRQ_TIMER)) {
    sched_fail("failed to unmask the timer interrupt");
    return -EFAULT;
  }

  // call the scheduler for the first time
  sched();
  return 0;
}

task_t *sched_find(pid_t pid) {
  slist_foreach(&task_head, task_t) if (cur->pid == pid) return cur;
  return NULL;
}

int32_t sched_exit(int32_t exit_code) {
  if (NULL == task_current)
    return -EINVAL;

  if (task_head == task_current) {
    panic("Attempted to kill init (exit code: %d)", exit_code);
    return 0;
  }

  sched_debg("exiting current task with %d", exit_code);

  task_current->exit_code = exit_code;
  task_current->state     = TASK_STATE_DEAD;

  // attach child processes to the PID 1
  dlist_foreach(&task_head, task_t) {
    if (cur->ppid == task_current->pid)
      cur->ppid = 1;
  }

  /*

   * we are currently running as the current task so we can't really
   * free it or remove it from the queue, that's done by the scheduler's
   * interrupt handler

   * so we call the handler to free the task and remove it from queue

  */
  sched();

  // this will never run
  return 0;
}

void sched_lock() {
  im_disable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_exception_handler);
  im_disable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_timer_handler);
}

void sched_unlock() {
  im_enable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_exception_handler);
  im_enable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_timer_handler);
}

/*

 * this is different from __sched_queue_next(), this function
 * is used to loop through all the tasks, so we don't really care
 * about their order

*/
task_t *sched_next(task_t *task) {
  if (NULL == task)
    return task_head;
  return task->next;
}
