#include "core/sched.h"
#include "core/pic.h"
#include "core/im.h"

#include "mm/vmm.h"
#include "mm/pm.h"

#include "boot/gdt.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/panic.h"
#include "util/list.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

task_t *current, *head, *idle;

#define sched_debg(f, ...) pdebg("Sched: (0x%x) " f, current, ##__VA_ARGS__)
#define sched_info(f, ...) pinfo("Sched: (0x%x) " f, current, ##__VA_ARGS__)
#define sched_fail(f, ...) pfail("Sched: (0x%x) " f, current, ##__VA_ARGS__)

#define __sched_is_valid_ring(ring) (ring == TASK_RING_USER || ring == TASK_RING_KERNEL)
#define __sched_add_list(task)      slist_add(&head, task, task_t)

#define __sched_update_task(task, stack)                                                                               \
  do {                                                                                                                 \
    task->regs.r15    = stack->r15;                                                                                    \
    task->regs.r14    = stack->r14;                                                                                    \
    task->regs.r13    = stack->r13;                                                                                    \
    task->regs.r12    = stack->r12;                                                                                    \
    task->regs.r11    = stack->r11;                                                                                    \
    task->regs.r9     = stack->r9;                                                                                     \
    task->regs.r8     = stack->r8;                                                                                     \
    task->regs.rdi    = stack->rdi;                                                                                    \
    task->regs.rsi    = stack->rsi;                                                                                    \
    task->regs.rbp    = stack->rbp;                                                                                    \
    task->regs.rsp    = stack->rsp;                                                                                    \
    task->regs.rdx    = stack->rdx;                                                                                    \
    task->regs.rcx    = stack->rcx;                                                                                    \
    task->regs.rbx    = stack->rbx;                                                                                    \
    task->regs.rax    = stack->rax;                                                                                    \
    task->regs.rip    = stack->rip;                                                                                    \
    task->regs.rflags = stack->rflags;                                                                                 \
    task->regs.cs     = stack->cs;                                                                                     \
    task->regs.ss     = stack->ss;                                                                                     \
  } while (0);

#define __sched_update_stack(task, stack)                                                                              \
  do {                                                                                                                 \
    stack->r15    = task->regs.r15;                                                                                    \
    stack->r14    = task->regs.r14;                                                                                    \
    stack->r13    = task->regs.r13;                                                                                    \
    stack->r12    = task->regs.r12;                                                                                    \
    stack->r11    = task->regs.r11;                                                                                    \
    stack->r9     = task->regs.r9;                                                                                     \
    stack->r8     = task->regs.r8;                                                                                     \
    stack->rdi    = task->regs.rdi;                                                                                    \
    stack->rsi    = task->regs.rsi;                                                                                    \
    stack->rbp    = task->regs.rbp;                                                                                    \
    stack->rsp    = task->regs.rsp;                                                                                    \
    stack->rdx    = task->regs.rdx;                                                                                    \
    stack->rcx    = task->regs.rcx;                                                                                    \
    stack->rbx    = task->regs.rbx;                                                                                    \
    stack->rax    = task->regs.rax;                                                                                    \
    stack->rip    = task->regs.rip;                                                                                    \
    stack->rflags = task->regs.rflags;                                                                                 \
    stack->cs     = task->regs.cs;                                                                                     \
    stack->ss     = task->regs.ss;                                                                                     \
  } while (0);

// idle task is executed when there is no other task to do
void *__sched_idle() {
  while (true)
    __asm__("hlt");
}

task_t *__sched_create(const char *name, uint8_t ring) {
  if (NULL == name) {
    sched_fail("invalid task name");
    return NULL;
  }

  if (!__sched_is_valid_ring(ring)) {
    sched_fail("invalid task ring (%u)", ring);
    return NULL;
  }

  // allocate the space for the task
  task_t *task = vmm_alloc(sizeof(task_t));

  if (NULL == task) {
    sched_fail("failed to allocate space for the new task");
    return NULL;
  }

  bzero(task, sizeof(task_t));

  // set the required values
  strncpy(task->name, name, NAME_MAX + 1);
  task->state = TASK_STATE_READY;
  task->prio  = TASK_PRIO_DEFAULT;
  task->ticks = TASK_TICKS_DEFAULT;
  task->ring  = TASK_RING_USER;

  // get the next ID for the task
  slist_foreach(&head, task_t) {
    if (cur->id > task->id)
      task->id = cur->id;
  }

  task->id++;
  return task;
}

task_t *__sched_find_next() {
  task_t *task = NULL;

  slist_foreach(&head, task_t) {
    if (cur->state != TASK_STATE_READY)
      continue;

    if (NULL == task || cur->prio > task->prio)
      task = cur;
  }

  return task;
}

bool __sched_switch(im_stack_t *stack) {
  if (NULL == current || NULL == stack)
    return false;

  __sched_update_stack(current, stack);
  current->state = TASK_STATE_ACTIVE;
  current->ticks = TASK_TICKS_DEFAULT;

  return true;
}

void __sched_handler(im_stack_t *stack) {
  task_t *new_task = NULL;

  if (NULL == current) {
    /*

     * if current == NULL, then this means this is the task that called sched_init()
     * which should be the main kernel task

     * so we create a new task from the current registers
     * and it's set as the current task, also registers are not modified,
     * so we are going to jump back to this new task

    */
    current = __sched_create("main", TASK_RING_USER);
    __sched_add_list(current);

    __sched_update_task(current, stack);
    current->state = TASK_STATE_ACTIVE;

    return;
  }

  // update the saved task registers
  __sched_update_task(current, stack);

  // try to find a new task if we are idling
  if (idle == current)
    goto find_new;

  switch (current->state) {
  case TASK_STATE_ACTIVE:
    // increment the priority of READY tasks
    slist_foreach(&head, task_t) {
      if (cur->state != TASK_STATE_READY)
        continue;

      if (cur->prio < TASK_PRIO_MAX)
        cur->prio++;
    }

    // decrement the proirity of the ACTIVE task
    if (current->prio > TASK_PRIO_MIN)
      current->prio--;

    // also decrement the remaning ticks
    if (current->ticks > UINT8_MIN)
      current->ticks--;

    /*

     * if we don't have anymore ticks, time to switch
     * to a ready task in the queue

    */
    else {
      current->state = TASK_STATE_READY;
      break;
    }

    return;

  case TASK_STATE_DEAD:
    // remove the task from the list
    slist_del(&head, current, task_t);

    // free the task structure
    vmm_free(current);
    break;
  }

find_new:
  // see if we need to switch to a new task and do so if needed
  if ((new_task = __sched_find_next()) == NULL) {
    if (current == idle)
      return;

    sched_debg("switching to the idle task");
    current = idle;

    __sched_switch(stack);
    return;
  }

  // switch to new task
  if (current != new_task) {
    sched_debg("switching to the new task (%s, 0x%x)",
        new_task->name,
        new_task->regs.rip); // only print if we are not the only task
    current = new_task;
    __sched_switch(stack);
  }
}

task_t *__sched_new(const char *name, void *func, uint8_t ring) {
  task_t *new = __sched_create(name, ring);

  if (NULL == new)
    return NULL;

  /*

   * bit 1 = reserved, 9 = interrupt enable
   * https://en.wikipedia.org/wiki/FLAGS_register

  */
  new->regs.rflags = ((1 << 1) | (1 << 9));
  new->regs.rip    = (uint64_t)func;

  switch (ring) {
  case TASK_RING_KERNEL:
    new->regs.cs  = gdt_offset(gdt_desc_code_0_addr);
    new->regs.ss  = gdt_offset(gdt_desc_data_0_addr);
    new->regs.rsp = (uint64_t)pm_alloc(TASK_STACK_SIZE);

    break;

  case TASK_RING_USER:
    new->regs.cs  = gdt_offset(gdt_desc_code_3_addr);
    new->regs.ss  = gdt_offset(gdt_desc_data_3_addr);
    new->regs.rsp = (uint64_t)pm_alloc(TASK_STACK_SIZE);

    /*

     * ORed with 3 to set the RPL to 3
     * see https://wiki.osdev.org/Segment_Selector

    */
    new->regs.cs |= 3;
    new->regs.ss |= 3;

    // make sure the allocated stack is accessable by ring 3
    pm_set_all(new->regs.rsp, TASK_STACK_SIZE, PM_ENTRY_FLAG_US);

    break;
  }

  // stack grows downwards
  new->regs.rsp += PM_PAGE_SIZE *TASK_STACK_SIZE;

  // return the new task ID
  return new;
}

int32_t sched_new(const char *name, void *func, uint8_t ring) {
  if (NULL == name || NULL == func || !__sched_is_valid_ring(ring))
    return -EINVAL;

  task_t *new = NULL;

  if (NULL == (new = __sched_new(name, func, ring)))
    return -ENOMEM;

  __sched_add_list(new);
  return new->id;
}

task_t *sched_find(int32_t id) {
  slist_foreach(&head, task_t) {
    if (cur->id == id)
      return cur;
  }

  return NULL;
}

int32_t sched_init() {
  current = head = NULL;

  // unmask the timer interrupt for the scheduler
  if (!pic_unmask(PIC_IRQ_TIMER))
    return -EFAULT;

  // add the scheduler handler
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), __sched_handler);

  // call the scheduler interrupt once to create the current task
  sched();

  // also create the idle task
  if ((idle = __sched_new("idle", __sched_idle, TASK_RING_KERNEL)) == NULL)
    panic("Failed to create the idle task");

  return 0;
}
