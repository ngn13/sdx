#pragma once
#include "types.h"

#define TASK_REG_COUNT        (20)
#define TASK_STACK_PAGE_COUNT (16) // 16 pages, 64KB

#define TASK_TICKS_DEFAULT (50)

#define TASK_PRIO_MAX     (63)
#define TASK_PRIO_MIN     (0)
#define TASK_PRIO_DEFAULT (20)

#define TASK_RING_KERNEL (0)
#define TASK_RING_USER   (3)

#ifndef __ASSEMBLY__
#include "sched/sched.h"

// copies im_stack_t to task_regs_t
#define __task_stack_to_regs(regs, stack)                                                                              \
  do {                                                                                                                 \
    regs->r15    = stack->r15;                                                                                         \
    regs->r14    = stack->r14;                                                                                         \
    regs->r13    = stack->r13;                                                                                         \
    regs->r12    = stack->r12;                                                                                         \
    regs->r11    = stack->r11;                                                                                         \
    regs->r9     = stack->r9;                                                                                          \
    regs->r8     = stack->r8;                                                                                          \
    regs->rdi    = stack->rdi;                                                                                         \
    regs->rsi    = stack->rsi;                                                                                         \
    regs->rbp    = stack->rbp;                                                                                         \
    regs->rsp    = stack->rsp;                                                                                         \
    regs->rdx    = stack->rdx;                                                                                         \
    regs->rcx    = stack->rcx;                                                                                         \
    regs->rbx    = stack->rbx;                                                                                         \
    regs->rax    = stack->rax;                                                                                         \
    regs->rip    = stack->rip;                                                                                         \
    regs->rflags = stack->rflags;                                                                                      \
    regs->cs     = stack->cs;                                                                                          \
    regs->ss     = stack->ss;                                                                                          \
  } while (0);

// copies task_regs_t to im_stack_t
#define __task_regs_to_stack(regs, stack)                                                                              \
  do {                                                                                                                 \
    stack->r15    = regs->r15;                                                                                         \
    stack->r14    = regs->r14;                                                                                         \
    stack->r13    = regs->r13;                                                                                         \
    stack->r12    = regs->r12;                                                                                         \
    stack->r11    = regs->r11;                                                                                         \
    stack->r9     = regs->r9;                                                                                          \
    stack->r8     = regs->r8;                                                                                          \
    stack->rdi    = regs->rdi;                                                                                         \
    stack->rsi    = regs->rsi;                                                                                         \
    stack->rbp    = regs->rbp;                                                                                         \
    stack->rsp    = regs->rsp;                                                                                         \
    stack->rdx    = regs->rdx;                                                                                         \
    stack->rcx    = regs->rcx;                                                                                         \
    stack->rbx    = regs->rbx;                                                                                         \
    stack->rax    = regs->rax;                                                                                         \
    stack->rip    = regs->rip;                                                                                         \
    stack->rflags = regs->rflags;                                                                                      \
    stack->cs     = regs->cs;                                                                                          \
    stack->ss     = regs->ss;                                                                                          \
  } while (0);

// different task states
enum {
  TASK_STATE_BUSY   = 0, // task is not ready to be scheduled
  TASK_STATE_READY  = 1, // task is ready to be scheduled
  TASK_STATE_ACTIVE = 2, // task is actively running (it is the "current" task)
  TASK_STATE_WAIT   = 3, // task is waiting for a child
  TASK_STATE_DEAD   = 4, // task is dead
};

// different task levels
enum {
  TASK_LEVEL_CRITIKAL = 0,
  TASK_LEVEL_HIGH     = 1,
  TASK_LEVEL_MEDIUM   = 2,
  TASK_LEVEL_LOW      = 3,
};

task_t *task_alloc();                                                           // allocate a new task
int32_t task_free(task_t *task);                                                // free a given task
int32_t task_update(task_t *task, const char *name, uint8_t ring, void *entry); // update task
task_t *task_copy(task_t *task);                                                // copy a given task (used for fork)

#define __task_update_regs(task, stack)                                                                                \
  __task_stack_to_regs((&task->regs), stack) // update task registers from the im_stack_t
#define __task_update_stack(task, stack)                                                                               \
  __task_regs_to_stack((&task->regs), stack) // update im_stack_t from task registers
#define __task_is_valid_ring(ring)                                                                                     \
  (ring == TASK_RING_USER || ring == TASK_RING_KERNEL) // check if a specified ring is valid

#define task_prio(task, p)  task->prio = p      // set the priority for a task
#define task_ticks(task, t) task->min_ticks = t // set the ticks (runtime) for a task

// set the level (priority and tick combination) for a task
#define task_level(task, l)                                                                                            \
  switch (l) {                                                                                                         \
  case TASK_LEVEL_CRITIKAL:                                                                                            \
    task_prio(task, TASK_PRIO_MAX);                                                                                    \
    task_ticks(task, UINT8_MAX);                                                                                       \
    break;                                                                                                             \
                                                                                                                       \
  case TASK_LEVEL_HIGH:                                                                                                \
    task_prio(task, TASK_PRIO_MAX / 2);                                                                                \
    task_ticks(task, 100);                                                                                             \
    break;                                                                                                             \
                                                                                                                       \
  case TASK_LEVEL_MEDIUM:                                                                                              \
    task_prio(task, TASK_PRIO_DEFAULT);                                                                                \
    task_ticks(task, TASK_TICKS_DEFAULT);                                                                              \
    break;                                                                                                             \
                                                                                                                       \
  case TASK_LEVEL_LOW:                                                                                                 \
    task_prio(task, 0);                                                                                                \
    task_ticks(task, 10);                                                                                              \
    break;                                                                                                             \
  }

#endif
