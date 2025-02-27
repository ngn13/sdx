#include "sched/sched.h"
#include "sched/task.h"

#include "util/string.h"
#include "mm/region.h"

#include "errno.h"
#include "types.h"

int32_t task_mem_del(task_t *task, region_t *reg) {
  if (NULL == task || NULL == reg)
    return -EINVAL;

  int32_t err = 0;

  if ((err = region_del(&task->mem, reg)) != 0) {
    sched_debg("failed to delete memory region @ 0x%p (%u pages): %s", reg->vaddr, reg->num, strerror(err));
    return err;
  }

  if ((err = region_unmap(reg)) != 0) {
    sched_debg("failed to unmap memory region @ 0x%p (%u pages): %s", reg->vaddr, reg->num, strerror(err));
    return err;
  }

  region_free(reg);
  return 0;
}
