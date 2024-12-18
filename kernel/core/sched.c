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
#include "util/asm.h"
#include "util/bit.h"

#include "errno.h"
#include "types.h"

task_t *current, *task_head, *task_idle;

#define sched_debg(f, ...) pdebg("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)
#define sched_info(f, ...) pinfo("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)
#define sched_fail(f, ...) pfail("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)

#define __sched_is_valid_ring(ring) (ring == TASK_RING_USER || ring == TASK_RING_KERNEL)
#define __sched_add(task)           slist_add(&task_head, task, task_t)
#define __sched_free(task)                                                                                             \
  do {                                                                                                                 \
    if (NULL != task) {                                                                                                \
      vmm_free(task->stack);                                                                                           \
      vmm_free(task);                                                                                                  \
    }                                                                                                                  \
  } while (0)
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

task_t *__sched_create(const char *name, uint8_t ring, im_stack_t *stack) {
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
  task->state = TASK_STATE_WAIT;
  task->prio  = TASK_PRIO_DEFAULT;
  task->ticks = TASK_TICKS_DEFAULT;
  task->ring  = ring;

  if (NULL != stack)
    __sched_update_task(task, stack);

  return task;
}

task_t *__sched_new(const char *name, void *func, uint8_t ring) {
  task_t *new = __sched_create(name, ring, NULL);

  if (NULL == new)
    return NULL;

  /*

   * bit 1 = reserved, 9 = interrupt enable
   * https://en.wikipedia.org/wiki/FLAGS_register

  */
  new->regs.rflags = ((1 << 1) | (1 << 9));
  new->regs.rip    = (uint64_t)func;
  new->regs.rsp    = (uint64_t)(new->stack = pm_alloc(TASK_STACK_PAGE_COUNT));

  switch (ring) {
  case TASK_RING_KERNEL:
    new->regs.cs = gdt_offset(gdt_desc_code_0_addr);
    new->regs.ss = gdt_offset(gdt_desc_data_0_addr);
    break;

  case TASK_RING_USER:
    new->regs.cs = gdt_offset(gdt_desc_code_3_addr);
    new->regs.ss = gdt_offset(gdt_desc_data_3_addr);

    /*

     * ORed with 3 to set the RPL to 3
     * see https://wiki.osdev.org/Segment_Selector

    */
    new->regs.cs |= 3;
    new->regs.ss |= 3;

    // make sure the allocated stack is accessable by ring 3
    pm_set_all(new->regs.rsp, TASK_STACK_PAGE_COUNT, PM_ENTRY_FLAG_US);

    break;
  }

  // stack grows downwards
  new->regs.rsp += PM_PAGE_SIZE *TASK_STACK_PAGE_COUNT;

  // return the new task ID
  sched_debg("created a new task");
  sched_debg("|- Name: %s", new->name);
  sched_debg("|- Ring: %u", new->ring);
  sched_debg("|- RIP: 0x%x", new->regs.rip);
  sched_debg("`- Stack: 0x%x", new->regs.rsp);
  return new;
}

task_t *__sched_next() {
  task_t *task = NULL;

  slist_foreach(&task_head, task_t) {
    if (cur->state != TASK_STATE_READY)
      continue;

    if (NULL == task || cur->prio > task->prio)
      task = cur;
  }

  return task;
}

bool __sched_switch(task_t *task, im_stack_t *stack) {
  if (NULL == task || NULL == stack)
    return false;

  if (NULL != current)
    current->state = TASK_STATE_READY;

  __sched_update_stack((current = task), stack);
  current->state = TASK_STATE_ACTIVE;
  current->ticks = TASK_TICKS_DEFAULT;

  return true;
}

void __sched_update_handler(im_stack_t *stack) {
  // simply update the registers for the current task
  if (NULL != current)
    __sched_update_task(current, stack);
}

void __sched_timer_handler(im_stack_t *stack) {
  task_t *new_task = NULL;

  if (NULL == current) {
    /*

     * if current == NULL, then this means this is the task that called sched_init()
     * which should be the main kernel task

     * so we create a new task from the current registers, and we switch to it
     * we also add the new task to the task list

    */
    new_task = __sched_create("main", TASK_RING_KERNEL, stack);
    __sched_add(new_task);
    __sched_switch(new_task, stack);

    return;
  }

  // try to find a new task if we are idling
  if (task_idle == current)
    goto find_new;

  switch (current->state) {
  case TASK_STATE_ACTIVE:
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
    slist_del(&task_head, current, task_t);

    // free the task structure
    __sched_free(current);
    break;
  }

find_new:
  // see if we need to switch to a new task and do so if needed
  if ((new_task = __sched_next()) == NULL) {
    if (current != task_idle) {
      sched_debg("switching to the idle task");
      __sched_switch(task_idle, stack);
    }
    return;
  }

  // switch to new task (if we have one)
  if (current != new_task) {
    sched_debg("switching to a new task: %s", new_task->name);
    __sched_switch(new_task, stack);
  }
}

void __sched_exception_handler(im_stack_t *stack) {
  if (NULL == current || TASK_STATE_ACTIVE != current->state)
    return;

  switch (stack->vector) {
  case IM_INT_DIV_ERR:
    sched_fail("received an division by zero exception");
    break;

  case IM_INT_INV_OPCODE:
    sched_fail("received an invalid opcode exception");
    break;

  case IM_INT_DOUBLE_FAULT:
    sched_fail("received a double fault exception");
    break;

  case IM_INT_GENERAL_PROTECTION_FAULT:
    sched_fail("received a general protection fault exception");
    break;

  case IM_INT_PAGE_FAULT:
    sched_fail("received an page fault exception");
    printf("      P=%u W=%u U=%u R=%u I=%u PK=%u SS=%u SGX=%u\n",
        bit_get(stack->error, 0),
        bit_get(stack->error, 1),
        bit_get(stack->error, 2),
        bit_get(stack->error, 3),
        bit_get(stack->error, 4),
        bit_get(stack->error, 5),
        bit_get(stack->error, 6),
        bit_get(stack->error, 7));
    break;

  default:
    sched_fail("received an unknown exception (0x%x)", stack->vector);
    break;
  }

  panic_with(&current->regs, "Task %s received a critikal exception", current->name);
}

task_t *sched_new(const char *name, void *func, uint8_t ring) {
  if (NULL == name || NULL == func || !__sched_is_valid_ring(ring))
    return NULL;

  task_t *new = __sched_new(name, func, ring);

  if (NULL == new)
    return NULL;

  __sched_add(new);
  return new;
}

int32_t sched_kill(task_t *task) {
  if (NULL == task)
    return -EINVAL;

  task->state = TASK_STATE_DEAD;

  if (current != task) {
    sched_debg("killing task 0x%x (%s)", task, task->name);
    slist_del(&task_head, task, task_t);
    __sched_free(task);
  }

  else
    sched_debg("killing current task");

  return 0;
}

int32_t sched_init() {
  current = task_idle = task_head = NULL;

  // also create the idle task
  if ((task_idle = __sched_new("idle", _hang, TASK_RING_KERNEL)) == NULL)
    panic("Failed to create the idle task");

  // and make sure it's ready
  sched_ready(task_idle);

  // add the scheduler handler
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), IM_HANDLER_PRIO_FIRST, __sched_update_handler);
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), IM_HANDLER_PRIO_SECOND, __sched_timer_handler);

  // add the exception handlers
  for (uint8_t i = 0; i < IM_INT_EXCEPTIONS; i++) {
    im_add_handler(i, IM_HANDLER_PRIO_FIRST, __sched_update_handler);
    im_add_handler(i, IM_HANDLER_PRIO_SECOND, __sched_exception_handler);
  }

  // unmask the timer interrupt for the scheduler
  if (!pic_unmask(PIC_IRQ_TIMER))
    return -EFAULT;

  // call the scheduler interrupt once to create the current task
  sched();

  return 0;
}
