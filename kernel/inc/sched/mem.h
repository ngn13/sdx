#pragma once

#ifndef __ASSEMBLY__
#include "sched/task.h"
#include "types.h"

#define task_mem_end(mem) (mem->addr + mem->count * PM_PAGE_SIZE) // get the end address for a memory region
int32_t task_mem_add(task_t *task, task_mem_type_t type, void *vaddr, uint64_t paddr,
    uint64_t num); // add a memory region to task's memory region list
int32_t task_mem_del(task_t *task, task_mem_type_t type,
    void *vaddr);                                // remove and free a memory region from the task's memory region list
int32_t task_mem_clear(task_t *task);            // free every memory region and clear the task's memory region list
int32_t task_mem_copy(task_t *to, task_t *from); // copy memory regions from one task to the other

#endif
