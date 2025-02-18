#pragma once
#ifndef __ASSEMBLY__

#include "util/printk.h"
#include "sched/task.h"
#include "core/pic.h"
#include "types.h"

#define sched_debg(f, ...) pdebg("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)
#define sched_info(f, ...) pinfo("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)
#define sched_fail(f, ...) pfail("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)
#define sched_warn(f, ...) pwarn("Sched: (0x%x:%s) " f, current, NULL == current ? NULL : current->name, ##__VA_ARGS__)

// current task
extern task_t *task_current;
#define current (task_current)

#define sched()        __asm__("int %0" ::"i"(pic_to_int(PIC_IRQ_TIMER))) // call the scheduler (with the timer interrupt)
#define sched_prio(p)  (task_current->prio = p)
#define sched_state(s) (task_current->state = s)
int32_t sched_init();                  // initialize the scheduler task list
task_t *sched_fork();                  // fork the current process
task_t *sched_find(pid_t pid);         // find a task by it's PID
int32_t sched_exit(int32_t exit_code); // exit the current task
void    sched_lock();                  // lock (disable) scheduling
void    sched_unlock();                // unlock (enable) scheduling

#endif
