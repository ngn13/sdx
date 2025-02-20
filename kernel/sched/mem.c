#include "sched/mem.h"
#include "sched/task.h"

#include "util/list.h"
#include "util/mem.h"

#include "mm/vmm.h"
#include "mm/heap.h"

#include "types.h"
#include "errno.h"

void __task_mem_free(task_mem_t *mem) {
  vmm_unmap(mem->vaddr, mem->num);
  heap_free(mem);
}

uint64_t __task_mem_flags(task_mem_t *mem) {
  uint64_t flags = VMM_FLAGS_DEFAULT;

  switch (mem->type) {
  case TASK_MEM_TYPE_CODE:
    break;

  case TASK_MEM_TYPE_STACK:
    flags |= VMM_FLAG_XD;
    break;

  case TASK_MEM_TYPE_HEAP:
    flags |= VMM_FLAG_XD;
    break;
  }

  if ((uint64_t)mem->vaddr < VMM_VMA_USER_END)
    flags |= VMM_FLAG_US;

  return flags;
}

int32_t task_mem_add(task_t *task, task_mem_type_t type, void *vaddr, uint64_t paddr, uint64_t num) {
  if (NULL == task || NULL == vaddr || num == 0)
    return -EINVAL;

  // allocate the region structure
  task_mem_t *region = heap_alloc(sizeof(task_mem_t));

  if (NULL == region)
    return -ENOMEM;

  bzero(region, sizeof(task_mem_t));
  region->type  = type;
  region->vaddr = vaddr;
  region->paddr = paddr;
  region->num   = num;

  // add memory region to the list
  slist_add(&task->mem, region, task_mem_t);
  return 0;
}

int32_t task_mem_del(task_t *task, task_mem_type_t type, void *vaddr) {
  if (NULL == task || NULL == vaddr)
    return -EINVAL;

  slist_foreach(&task->mem, task_mem_t) {
    if (vaddr != NULL && cur->vaddr != vaddr)
      continue;

    if (type != 0 && cur->type != type)
      continue;

    vmm_save();
    task_vmm_switch(task);

    __task_mem_free(cur);
    slist_del(&task->mem, cur, task_mem_t);

    vmm_restore();
    return 0;
  }

  return -EFAULT;
}

int32_t task_mem_clear(task_t *task) {
  vmm_save();
  task_vmm_switch(task);

  slist_clear(&task->mem, __task_mem_free, task_mem_t);

  vmm_restore();
  return 0;
}

int32_t task_mem_copy(task_t *to, task_t *from) {
  void    *from_vaddr = NULL, *to_vaddr = NULL;
  uint64_t from_num = 0;
  int32_t  err      = 0;

  vmm_save();
  task_vmm_switch(to);

  slist_foreach(&from->mem, task_mem_t) {
    from_num = cur->num;

    // temporarily map the region of the source task (from)
    if (NULL == (from_vaddr = vmm_map_to_paddr(cur->paddr, cur->num, VMM_VMA_KERNEL, VMM_FLAGS_DEFAULT))) {
      err = -ENOMEM;
      goto end;
    }

    // map same virtual memory region to destination task (to)
    if (NULL == (to_vaddr = vmm_map_to_vaddr((uint64_t)cur->vaddr, from_num, 0, __task_mem_flags(cur)))) {
      err = -ENOMEM;
      goto end;
    }

    /*

     * copy memory region's contents from the source task's memory region
     * to the destination task's memory region

    */
    memcpy(to_vaddr, from_vaddr, from_num * VMM_PAGE_SIZE);

    // unmap the temporarily mapped source task's memory region
    vmm_unmap(from_vaddr, from_num);
    from_vaddr = NULL;

    // add mapped memory to the destination task's memory region list
    if ((err = task_mem_add(to, cur->type, to_vaddr, vmm_resolve(to_vaddr), from_num)) != 0)
      goto end;

    // make sure we don't unmap added region if we jump to end
    to_vaddr = NULL;
  }

end:
  if (NULL != from_vaddr)
    vmm_unmap(from_vaddr, from_num);

  if (NULL != to_vaddr)
    vmm_unmap(to_vaddr, from_num);

  vmm_restore();
  return err;
}
