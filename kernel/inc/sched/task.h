#pragma once

#include "mm/vmm.h"
#include "limits.h"
#include "types.h"

#define TASK_REG_COUNT        (20)
#define TASK_STACK_PAGE_COUNT (16) // 16 pages, 64KB
#define TASK_TICKS_DEFAULT    (20)

#define TASK_PRIO_MAX (63)
#define TASK_PRIO_MIN (1)

#ifndef __ASSEMBLY__

// different task states
enum {
  TASK_STATE_READY, // task is ready to run
  TASK_STATE_SAVE,  // task should be saved, don't modify the registers
  TASK_STATE_WAIT,  // task is waiting on something, should be moved to end of to the queue
  TASK_STATE_DEAD,  // task is dead, should be removed from the queue
};

// different task priorities
enum {
  TASK_PRIO_LOW = 1,
  TASK_PRIO_HIGH,
  TASK_PRIO_CR1TIKAL,
};

// task memory region types
typedef enum {
  TASK_MEM_TYPE_CODE = 1, // memory region contains runable code
  TASK_MEM_TYPE_STACK,    // memory region contains program stack
  TASK_MEM_TYPE_HEAP,     // memory region contains heap memory
} task_mem_type_t;

// task memory region
typedef struct task_mem {
  task_mem_type_t  type;  // memory region type (what it's used for)
  void            *vaddr; // memory region virtual start address
  uint64_t         paddr; // memory region physical start address
  uint64_t         num;   // number of pages in the region
  struct task_mem *next;  // next memory region
} task_mem_t;

// task signal set strucure (signal list)
typedef struct task_sigset {
  int32_t             value;
  struct task_sigset *next;
} task_sigset_t;

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
  pid_t pid, ppid;          // PID and parent PID

  task_regs_t regs;      // saved task registers
  uint8_t     ticks;     // current tick counter for this task
  uint8_t     state : 4; // state of this task (see the enum above)
  uint8_t     prio  : 6; // task priority (also sse the enum above)

  task_sighand_t sighand[SIG_MAX]; // signal handlers
  task_sigset_t *signal;           // signal queue

  int32_t exit_code; // exit code for the task
  int32_t wait_code; // exit code for the task the current task is waiting for

  void       *vmm; // VMM used for this task
  task_mem_t *mem; // memory region list

  struct task *next; // next task in the task queue
  struct task *prev; // previous task in the task queue
} task_t;

task_t *task_create(task_t *task);                   // create a new task, if a task is provided copy it
int32_t task_rename(task_t *task, const char *name); // rename the task
int32_t task_free(task_t *task);                     // free a given task

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

#define task_ticks(task)               (task->prio * TASK_TICKS_DEFAULT)
#define task_update_regs(task, stack)  __stack_to_regs((&task->regs), stack) // update task registers from the im_stack_t
#define task_update_stack(task, stack) __regs_to_stack((&task->regs), stack) // update im_stack_t from task registers
#define task_sigset_empty(task)        (NULL == task->signal)
#define task_vmm_switch(task)                                                                                          \
  do {                                                                                                                 \
    vmm_sync(task->vmm);                                                                                               \
    vmm_switch(task->vmm);                                                                                             \
  } while (0)

#endif
