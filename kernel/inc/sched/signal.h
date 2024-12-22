#pragma once

#ifndef __ASSEMBLY__
#include "sched/task.h"
#include "limits.h"
#include "types.h"

extern sighand_t sigdfl[SIG_MAX];
void             sighand_term(int32_t sig);
void             sighand_dump(int32_t sig);

int32_t task_signal_pop(task_t *task);                                              // get next signal in the queue
#define task_signal_clear(task) slist_clear(&task->signal, __sigset_free, sigset_t) // free the signal queue
void task_signal_call(task_t *task, int32_t sig);                                   // call the task signal handler

int32_t task_signal(task_t *task, int32_t sig, sighand_t hand); // set a signal handler for the task
int32_t task_kill(task_t *task, int32_t sig);                   // send a signal to the task

#define __sigset_free(ss)          vmm_free(ss)
#define __signal_can_ignore(sig)   (sig != SIGKILL)
#define __signal_call_default(sig) sigdfl[sig - 1](sig)

#endif
