#pragma once

#include "mm/region.h"
#include "mm/heap.h"

#include "limits.h"
#include "types.h"

#define TASK_STACK_PAGE_COUNT (4) // 4 pages, 16KB
#define TASK_REG_COUNT        (20)
#define TASK_TICKS_DEFAULT    (20)

#define TASK_PRIO_MAX (63)
#define TASK_PRIO_MIN (1)

#ifndef __ASSEMBLY__

// different task states
enum {
  TASK_STATE_HOLD,  // task is on holding the scheduler, keep it running
  TASK_STATE_READY, // task is ready to run
  TASK_STATE_SAVE,  // task should be saved, don't modify the registers
  TASK_STATE_WAIT,  // task is waiting on something, should be moved to end of to the queue
  TASK_STATE_DEAD,  // task is dead, should be removed from the queue
  TASK_STATE_FORK,  // task should be forked
};

// different task priorities
enum {
  TASK_PRIO_LOW = 1,
  TASK_PRIO_HIGH,
  TASK_PRIO_CR1TIKAL,
};

// task signal set structure (signal list)
typedef struct task_sigset {
  int32_t             value;
  struct task_sigset *next;
} task_sigset_t;

// task wait queue structure (wait list)
typedef struct task_waitq {
  int32_t            pid;    // PID of the child process that commited suicide
  int32_t            status; // status of the child process
  struct task_waitq *next;
} task_waitq_t;

// task signal handler
typedef void (*task_sighand_t)(int32_t);

// structure used to save the task registers
typedef struct {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t rip;
  uint64_t cs;
  uint64_t ss;
  uint64_t rflags;
  uint64_t rsp;
} __attribute__((packed)) task_regs_t;

// task structure
typedef struct task {
  char  name[NAME_MAX + 1]; // task name
  pid_t pid, ppid, cpid;    // PID, parent PID and last child PID

  task_regs_t regs;      // saved task registers
  uint8_t     ticks;     // current tick counter for this task
  uint8_t     state : 4; // state of this task (see the enum above)
  uint8_t     prio  : 6; // task priority (also sse the enum above)

  task_sighand_t sighand[SIG_MAX]; // signal handlers
  task_sigset_t *signal;           // signal queue

  task_waitq_t *waitq_head; // wait queue head
  task_waitq_t *waitq_tail; // wait queue tail

  int32_t term_code; // termination code (signal)
  int32_t exit_code; // exit code for the task

  region_t *mem; // memory region list
  void     *vmm; // VMM used for this task
  bool      old; // is the VMM up-to-date

  struct task *next; // next task in the task queue
  struct task *prev; // previous task in the task queue
} task_t;

task_t *task_new();                                  // create a new task
task_t *task_copy();                                 // copy the task
void    task_free(task_t *task);                     // free a given task
int32_t task_switch(task_t *task);                   // switch to given task's VMM
int32_t task_rename(task_t *task, const char *name); // rename the task

// sched/mem.c
#define task_mem_add(task, reg) (region_add(&task->mem, reg)) // add a memory region to task's memory region list
#define task_mem_find(task, type, vma)                                                                                 \
  (region_find(&task->mem, type, vma)) // find the given memory region from task's memory region list
int32_t task_mem_del(
    task_t *task, region_t *reg); // remove and unmap a memory region from the task's memory region list

// sched/signal.c
int32_t task_signal_setup();                                             // setup the default signal handlers
int32_t task_signal_set(task_t *task, int32_t sig, task_sighand_t hand); // set a signal handler for the task
int32_t task_signal_add(task_t *task, int32_t sig);                      // add a signal to the task's signal queue
int32_t task_signal_pop(task_t *task);                                   // get and handle the next signal in the queue
#define task_signal_clear(task) slist_clear(&(task)->signal, heap_free, task_sigset_t) // free the entire signal queue

// sched/waitq.c
int32_t       task_waitq_add(task_t *task, task_t *child); // create a new waitq from the child and add to task's waitq
task_waitq_t *task_waitq_pop(task_t *task);                // get next waitq in the task's wait queue
#define task_waitq_free(waitq)    (heap_free(waitq))       // free a waitq object
#define task_waitq_clear(task)    slist_clear(&(task)->waitq_head, task_waitq_free, task_waitq_t) // free the entire waitq
#define task_waitq_is_empty(task) ((task)->waitq_head == NULL) // check if the waitq is empty

// sched/stack.c
int32_t  task_stack_alloc(task_t *task);                           // allocate a stack for the given task
uint64_t task_stack_add(task_t *task, void *value, uint64_t size); // add a value to the task's stack
int32_t task_stack_add_list(task_t *task, char *list[], uint64_t limit, void **stack); // add a list to the task's stack
void   *task_stack_get(task_t *task, uint8_t vma);

// copies im_stack_t to task_regs_t
#define __stack_to_regs(regs, stack)                                                                                   \
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
  } while (0)

// copies task_regs_t to im_stack_t
#define __regs_to_stack(regs, stack)                                                                                   \
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
  } while (0)

#define task_ticks_reset(task)         (task->ticks = task->prio * TASK_TICKS_DEFAULT) // reset the tick counter
#define task_update_regs(task, stack)  __stack_to_regs((&task->regs), stack) // update task registers from the im_stack_t
#define task_update_stack(task, stack) __regs_to_stack((&task->regs), stack) // update im_stack_t from task registers
#define task_sigset_empty(task)        (NULL == task->signal)

#endif
