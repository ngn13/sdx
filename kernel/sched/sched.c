#include "sched/sched.h"
#include "sched/signal.h"
#include "sched/task.h"

#include "core/im.h"
#include "core/pic.h"

#include "mm/vmm.h"
#include "mm/pm.h"

#include "boot/gdt.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/panic.h"
#include "util/list.h"
#include "util/lock.h"
#include "util/mem.h"
#include "util/asm.h"
#include "util/bit.h"

#include "errno.h"
#include "types.h"

task_t    *current = NULL, *task_head = NULL, *task_idle = NULL;
spinlock_t sched_spinlock;

#define __sched_print_task(task)                                                                                       \
  do {                                                                                                                 \
    sched_debg("|- Name : %s", task->name);                                                                            \
    sched_debg("|- PID  : %d", task->pid);                                                                             \
    sched_debg("|- Ring : %u", task->ring);                                                                            \
    sched_debg("|- RIP  : 0x%x", task->regs.rip);                                                                      \
    sched_debg("`- Stack: 0x%x", task->regs.rsp);                                                                      \
  } while (0)
#define __sched_list_add(task) slist_add(&task_head, task, task_t) // add a task to the task list
#define __sched_list_del(task) slist_del(&task_head, task, task_t) // delete a task from the task list

// find the next available PID
void __sched_pid(task_t *task) {
  task->pid = 0;

  slist_foreach(&task_head, task_t) {
    if (cur->pid > task->pid)
      task->pid = cur->pid;
  }

  if (task->pid == PID_MAX)
    panic("Reached the max PID");

  task->pid++;
}

void __sched_clean() {
  slist_foreach(&task_head, task_t) {
    if (cur == current)
      continue;

    if (cur->state == TASK_STATE_DEAD) {
      slist_del(&task_head, cur, task_t);
      break;
    }
  }
}

// switch to a given task by updating interrupt stack
void __sched_switch(task_t *task, im_stack_t *stack) {
  if (NULL != current && TASK_STATE_ACTIVE == current->state)
    current->state = TASK_STATE_READY;

  __task_update_stack((current = task), stack);
  current->state = TASK_STATE_ACTIVE;
  current->ticks = current->min_ticks;
}

// switch to the next available task
bool __sched_switch_next(im_stack_t *stack) {
  task_t *next_task = NULL;

  // search for an available task
  slist_foreach(&task_head, task_t) {
    if (cur->state != TASK_STATE_READY)
      continue;

    if (NULL == next_task || cur->prio > next_task->prio)
      next_task = cur;
  }

  // no available task? then we'll idle
  if (NULL == next_task)
    next_task = task_idle;

  // no need to switch to the task if it's the current task
  if (current != next_task) {
    sched_debg("switching to a new task: %s", next_task->name);
    __sched_switch(next_task, stack);
  }

  return true;
}

void __sched_update_handler(im_stack_t *stack) {
  /*

   * simply update the registers for the current task
   * if the task is still in ready state tho, we don't update the regitsers

  */
  if (NULL != current && TASK_STATE_READY != current->state)
    __task_update_regs(current, stack);
}

void __sched_signal_handler(im_stack_t *stack) {
  if (NULL == current)
    return;

  int32_t signal = task_signal_pop(current);
  task_signal_call(current, signal);
}

void __sched_timer_handler(im_stack_t *stack) {
  task_t *new_task = NULL;

  // clean the task list (remove dead task)
  __sched_clean();

  if (NULL == current) {
    /*

     * if current == NULL, then this means this is the task that called sched_init()
     * which should be the main kernel task

     * so we create a new task from the current registers, and we switch to it
     * we also add the new task to the task list

    */

    // create the new task
    task_update((new_task = task_alloc()), "main", TASK_RING_KERNEL, NULL);
    __task_update_regs(new_task, stack);

    __sched_pid(new_task);           // obtain a PID for the task
    __sched_list_add(new_task);      // add task to the task list
    __sched_switch(new_task, stack); // switch to the new task

    return;
  }

  else if (task_idle == current) {
    // if we are idling, then we should try to get a new task as soon as possible
    __sched_switch_next(stack);
    return;
  }

  switch (current->state) {
  case TASK_STATE_READY:
    // take the task into active state again and update the stack
    current->state = TASK_STATE_ACTIVE;
    __task_update_stack(current, stack);

  case TASK_STATE_ACTIVE:
    /*

     * if the current task is still active, update tasks' prios
     * and update current tasks remaning tick count

     * if the current task ran out of ticks, switch to the next task

    */

    // increment the priority of READY tasks
    slist_foreach(&task_head, task_t) {
      if (cur->state != TASK_STATE_READY)
        continue;

      if (cur->prio < TASK_PRIO_MAX)
        cur->prio++;
    }

    // decrement the proirity of the ACTIVE task
    if (current->prio > TASK_PRIO_MIN)
      current->prio--;

    // also decrement the remaning ticks
    if (current->ticks > UINT8_MIN) {
      current->ticks--;
      return;
    }

    // no more ticks? switch to the next task
    current->state = TASK_STATE_READY;
    break;

  case TASK_STATE_DEAD:
  case TASK_STATE_WAIT:
    /*

     * if the current task is waiting for a child, or if the current task is dead
     * then we can just switchto a new task

    */
    break;

  default:
    // something went wrong
    panic("Unexpected task state: %u (PID: %d)", current->state, current->pid);
  }

  // switch to a new task
  __sched_switch_next(stack);
}

void __sched_exception_handler(im_stack_t *stack) {
  if (NULL == current || TASK_STATE_ACTIVE != current->state)
    return;

  switch (stack->vector) {
  case IM_INT_DIV_ERR:
    sched_fail("#DE fault at 0x%x", current->regs.rip);
    task_kill(current, SIGSEGV);
    break;

  case IM_INT_INV_OPCODE:
    sched_fail("#UD fault at 0x%x", current->regs.rip);
    task_kill(current, SIGILL);
    break;

  case IM_INT_DOUBLE_FAULT:
    sched_fail("#DF abort at 0x%x", current->regs.rip);
    task_kill(current, SIGSEGV);
    break;

  case IM_INT_GENERAL_PROTECTION_FAULT:
    sched_fail("#GP fault at 0x%x", current->regs.rip);
    task_kill(current, SIGSEGV);
    break;

  case IM_INT_PAGE_FAULT:
    sched_fail("#PF fault at 0x%x", current->regs.rip);
    printf("      P=%u W=%u U=%u R=%u I=%u PK=%u SS=%u SGX=%u\n",
        bit_get(stack->error, 0),
        bit_get(stack->error, 1),
        bit_get(stack->error, 2),
        bit_get(stack->error, 3),
        bit_get(stack->error, 4),
        bit_get(stack->error, 5),
        bit_get(stack->error, 6),
        bit_get(stack->error, 7));
    task_kill(current, SIGSEGV);
    break;

  default:
    sched_fail("unknown fault (0x%x) at 0x%x", stack->vector, current->regs.rip);
    task_kill(current, SIGSEGV);
    break;
  }
}

int32_t sched_init() {
  current = task_head = task_idle = NULL;

  // add the scheduler handler
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), IM_HANDLER_PRIO_FIRST, __sched_update_handler);
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), IM_HANDLER_PRIO_SECOND, __sched_timer_handler);
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), IM_HANDLER_PRIO_SECOND, __sched_signal_handler);

  // add the exception handlers
  for (uint8_t i = 0; i < IM_INT_EXCEPTIONS; i++) {
    im_add_handler(i, IM_HANDLER_PRIO_FIRST, __sched_update_handler);
    im_add_handler(i, IM_HANDLER_PRIO_SECOND, __sched_timer_handler);
    im_add_handler(i, IM_HANDLER_PRIO_SECOND, __sched_signal_handler);
    im_add_handler(i, IM_HANDLER_PRIO_SECOND, __sched_exception_handler);
  }

  // unmask the timer interrupt for the scheduler
  if (!pic_unmask(PIC_IRQ_TIMER))
    return -EFAULT;

  // setup the default signal handlers
  sigdfl[SIGHUP - 1]  = sighand_term;
  sigdfl[SIGINT - 1]  = sighand_term;
  sigdfl[SIGILL - 1]  = sighand_dump;
  sigdfl[SIGKILL - 1] = sighand_term;
  sigdfl[SIGSEGV - 1] = sighand_dump;

  // setup the idle task
  task_update((task_idle = task_alloc()), "idle", TASK_RING_KERNEL, _hang);

  // call the scheduler interrupt once to create the current task
  sched();

  return 0;
}

task_t *sched_new(const char *name, uint8_t ring, void *func) {
  if (NULL == name || !__task_is_valid_ring(ring) || NULL == func)
    return NULL;

  task_t *new = task_alloc();

  if (task_update(new, name, ring, func) < 0)
    return NULL;

  // obtain a new PID and set the parent PID
  __sched_pid(new);
  new->ppid = current->pid;

  // add new task to the task list
  __sched_list_add(new);

  // print some debugging information about the task
  sched_debg("created a new task");
  __sched_print_task(new);

  return new;
}

task_t *sched_fork() {
  if (NULL == current || current->state != TASK_STATE_ACTIVE)
    return NULL;

  // copy current task
  task_t *new = task_copy(current);

  if (NULL == new)
    return NULL;

  // obtain a new PID and set the parent PID
  __sched_pid(new);
  new->ppid = current->pid;

  // add new task to the task list
  __sched_list_add(new);

  // print some debugging information about the task
  sched_debg("forkted the current task");
  __sched_print_task(new);

  return new;
}

int32_t sched_exit(int32_t exit_code) {
  if (NULL == current || current->state != TASK_STATE_ACTIVE)
    return -EINVAL;

  if (task_head == current || current->pid == 1)
    panic("Attempted to kill init (exit code: %d)", exit_code);

  sched_debg("exiting current task");

  current->exit_code = exit_code;       // set the exit code
  current->state     = TASK_STATE_DEAD; // mark the task as dead

  // take parent out of wait state if it's waiting for a child
  task_t *parent = sched_find(current->ppid);

  if (NULL != parent && parent->state == TASK_STATE_WAIT) {
    parent->wait_code = exit_code;
    parent->state     = TASK_STATE_READY;
  }

  // attach child processes to the PID 1
  slist_foreach(&task_head, task_t) {
    if (cur->ppid == current->pid)
      cur->ppid = 1;
  }

  // we cannot free the task right now, we are still using it
  return 0;
}

task_t *sched_find(pid_t pid) {
  slist_foreach(&task_head, task_t) {
    if (cur->pid == pid)
      return cur;
  }

  return NULL;
}

task_t *sched_next(task_t *task) {
  if (NULL == task)
    return task_head;
  return task->next;
}

void sched_lock() {
  if (!spinlock_is_locked(sched_spinlock)) {
    im_disable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_update_handler);
    im_disable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_timer_handler);
    im_disable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_signal_handler);
  }

  spinlock_lock(sched_spinlock);
}

void sched_unlock() {
  spinlock_unlock(sched_spinlock);

  if (!spinlock_is_locked(sched_spinlock)) {
    im_enable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_update_handler);
    im_enable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_timer_handler);
    im_enable_handler(pic_to_int(PIC_IRQ_TIMER), __sched_signal_handler);
  }
}
