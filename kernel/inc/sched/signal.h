#pragma once

#ifndef __ASSEMBLY__
#include "sched/task.h"
#include "types.h"

int32_t task_signal_setup();             // setup the default signal handlers
int32_t task_signal_clear(task_t *task); // clear (free) the signal queue
int32_t task_signal_pop(task_t *task);   // get next signal in the queue

int32_t task_signal(task_t *task, int32_t sig, task_sighand_t hand); // set a signal handler for the task
int32_t task_kill(task_t *task, int32_t sig);                        // send a signal to the task

#endif
