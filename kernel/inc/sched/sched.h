#pragma once
#ifndef __ASSEMBLY__

#include "util/printk.h"
#include "sched/task.h"
#include "core/im.h"

#include "types.h"

#define sched_debg(f, ...) pdebg("Sched: " f, ##__VA_ARGS__)
#define sched_info(f, ...) pinfo("Sched: " f, ##__VA_ARGS__)
#define sched_fail(f, ...) pfail("Sched: " f, ##__VA_ARGS__)
#define sched_warn(f, ...) pwarn("Sched: " f, ##__VA_ARGS__)

// current task
#define SCHED_INT (IM_INT_MAX)
extern task_t *task_current;
#define current (task_current)

#define sched()                                                                                                        \
  __asm__("mov %%rsp, %%rax\n"                                                                                         \
          "mov %0, %%rsp\n"                                                                                            \
          "push %%rax\n"                                                                                               \
          "int %1\n"                                                                                                   \
          "pop %%rsp\n" ::"r"((uint64_t)im_stack()),                                                                   \
      "i"(SCHED_INT)                                                                                                   \
      : "rax")
#define sched_prio(p)  (task_current->prio = p)
#define sched_state(s) (task_current->state = s)
#define sched_sleep(t) (task_current->sleep = t)
#define sched_wait()                                                                                                   \
  do {                                                                                                                 \
    sched_state(TASK_STATE_WAIT);                                                                                      \
    sched();                                                                                                           \
  } while (0)
#define sched_hold() sched_state(TASK_STATE_HOLD)
#define sched_done() sched_state(TASK_STATE_SAVE)
int32_t sched_init();                             // initialize the scheduler
task_t *sched_find(pid_t pid);                    // find a task by it's PID
int32_t sched_exit(int32_t exit_code);            // exit the current task
task_t *sched_next(task_t *task);                 // get the next task in the task list
task_t *sched_child(task_t *task, task_t *child); // get the next child of the task
#define sched_foreach() for (task_t *cur = sched_next(NULL); cur != NULL; cur = sched_next(cur))

#endif
