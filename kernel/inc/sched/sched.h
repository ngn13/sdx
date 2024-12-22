#pragma once
#ifndef __ASSEMBLY__
#include "util/printk.h"
#include "core/pic.h"

#include "limits.h"
#include "types.h"

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

// task memory region
typedef struct task_mem {
  void            *addr;  // memory region start address
  uint64_t         count; // memory region page count
  struct task_mem *next;  // next memory region
} task_mem_t;

// task signal set strucure (signal list)
typedef struct sigset {
  int32_t        value;
  struct sigset *next;
} sigset_t;

// signal handler
typedef void (*sighand_t)(int32_t);

// task structure
typedef struct task {
  char  name[NAME_MAX + 1]; // task name
  pid_t pid;                // PID
  pid_t ppid;               // parent PID

  task_regs_t regs;      // saved task registers
  uint8_t     min_ticks; // min ticks (runtime) for this task
  uint8_t     ticks;     // current tick counter for this task
  uint8_t     state : 4; // state of this task (see the enum above)
  uint8_t     prio  : 6; // task priority
  uint8_t     ring  : 2; // ring (privilege level) for this task

  sighand_t sighand[SIG_MAX]; // signal handlers
  sigset_t *signal;           // signal queue

  int32_t exit_code; // exit code for the task
  int32_t wait_code; // exit code for the waited process
  struct {
    void *kernel; // stack for the task (ring 0)
    void *user;   // stack for the task (ring 3)
  } stack;
  task_mem_t *mem; // memory region list

  struct task *next; // next task in the linked list
} task_t;

extern task_t *current; // current task

#define sched_debg(f, ...) pdebg("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)
#define sched_info(f, ...) pinfo("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)
#define sched_fail(f, ...) pfail("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)

#define sched() __asm__("int %0" ::"i"(pic_to_int(PIC_IRQ_TIMER)))
int32_t sched_init();                                          // initialize the scheduler task list
task_t *sched_new(const char *name, uint8_t ring, void *func); // create a new task
task_t *sched_fork();                                          // fork the current process
int32_t sched_exit(int32_t exit_code);                         // exit current task
task_t *sched_find(pid_t pid);                                 // find a task by it's PID
task_t *sched_next(task_t *task);                              // get the next task in the task list
void    sched_lock();                                          // lock (disable) scheduling
void    sched_unlock();                                        // unlock (enable) scheduling

#endif
