#pragma once

#ifndef __ASSEMBLY__
#include "sched/task.h"
#include "types.h"

int32_t task_mem_add(task_t *task, void *addr, uint64_t count); // add a memory region to task's memory region list
int32_t task_mem_del(task_t *task, void *addr); // remove and free a memory region from the task's memory region list
#define task_mem_clear(task)                                                                                           \
  slist_clear(                                                                                                         \
      &task->mem, __task_mem_free, task_mem_t) // free every memory region and clear the task's memory region list

// free memory pages specified in a task_mem_t structure, and free the structure itself
#define __task_mem_free(mem)                                                                                           \
  do {                                                                                                                 \
    pm_free(mem->addr, mem->count);                                                                                    \
    vmm_free(mem);                                                                                                     \
  } while (0);
#define __task_mem_end(mem) (mem->addr + mem->count * PM_PAGE_SIZE) // get the end address for a memory region

#endif
