#pragma once
#ifndef __ASSEMBLY__

#include "util/printk.h"
#include "sched/task.h"
#include "core/im.h"
#include "core/pic.h"
#include "types.h"

#define sched_debg(f, ...) pdebg("Sched: " f, ##__VA_ARGS__)
#define sched_info(f, ...) pinfo("Sched: " f, ##__VA_ARGS__)
#define sched_fail(f, ...) pfail("Sched: " f, ##__VA_ARGS__)
#define sched_warn(f, ...) pwarn("Sched: " f, ##__VA_ARGS__)

// current task
extern task_t *task_current;
#define current (task_current)

#define sched()                                                                                                        \
  __asm__("mov %%rsp, %%rax\n"                                                                                         \
          "mov %0, %%rsp\n"                                                                                            \
          "push %%rax\n"                                                                                               \
          "int %1\n"                                                                                                   \
          "pop %%rsp\n" ::"r"((uint64_t)im_stack()),                                                                   \
      "i"(pic_to_int(PIC_IRQ_TIMER))                                                                                   \
      : "rax")
#define sched_prio(p)  (task_current->prio = p)
#define sched_state(s) (task_current->state = s)
int32_t sched_init();                             // initialize the scheduler
task_t *sched_find(pid_t pid);                    // find a task by it's PID
int32_t sched_exit(int32_t exit_code);            // exit the current task
void    sched_lock();                             // lock (disable) scheduling
void    sched_unlock();                           // unlock (enable) scheduling
task_t *sched_next(task_t *task);                 // get the next task in the task list
task_t *sched_child(task_t *task, task_t *child); // get the next child of the task

#endif
