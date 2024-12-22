#include "sched/sched.h"
#include "sched/mem.h"

#include "mm/vmm.h"
#include "mm/pm.h"

#include "util/list.h"
#include "util/mem.h"

#include "types.h"
#include "errno.h"

int32_t task_mem_add(task_t *task, void *addr, uint64_t count) {
  if (NULL == task || NULL == addr || count == 0)
    return -EINVAL;

  // allocate the region structure
  task_mem_t *region = vmm_alloc(sizeof(task_mem_t));

  if (NULL == region)
    return -ENOMEM;

  bzero(region, sizeof(task_mem_t));
  region->addr  = addr;
  region->count = count;

  // apply page flags based on the task ring
  switch (task->ring) {
  case TASK_RING_KERNEL:
    // make sure the pages are not accessable by ring 3
    pm_clear((uint64_t)region->addr, region->count, PM_ENTRY_FLAG_US);
    break;

  case TASK_RING_USER:
    // make sure the pages are accessable by ring 3
    pm_set_all((uint64_t)region->addr, region->count, PM_ENTRY_FLAG_US);
    break;
  }

  // add memory region to the list
  slist_add(&task->mem, region, task_mem_t);

  return 0;
}

int32_t task_mem_del(task_t *task, void *addr) {
  if (NULL == task || NULL == addr)
    return -EINVAL;

  slist_foreach(&task->mem, task_mem_t) {
    if (cur->addr == addr) {
      __task_mem_free(cur);
      slist_del(&task->mem, cur, task_mem_t);
      return 0;
    }
  }

  return 0;
}
