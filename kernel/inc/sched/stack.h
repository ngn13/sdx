#pragma once
#ifndef __ASSEMBLY__
#include "sched/task.h"

int32_t  task_stack_alloc(task_t *task);                           // allocate a stack for the given task
uint64_t task_stack_add(task_t *task, void *value, uint64_t size); // add a value to the task's stack
int32_t task_stack_add_list(task_t *task, char *list[], uint64_t limit, void **stack); // add a list to the task's stack
uint64_t task_stack_get(task_t *task, uint64_t vma);

#endif
