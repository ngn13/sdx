#pragma once
#ifndef __ASSEMBLY__
#include "sched/task.h"

int32_t  task_stack_alloc(task_t *task);                            // allocate a stack for the given task
uint64_t task_stack_copy(task_t *task, void *value, uint64_t size); // copy a value to the current stack (rsp)
int32_t  task_stack_copy_list(
     task_t *task, char *list[], uint64_t limit, void **stack); // copy a list to the current stack
void     task_stack_free(task_t *task);                         // free the stack for a given task
uint64_t task_stack_get(task_t *task, uint8_t ring);            // get the stack for a ring

#endif
