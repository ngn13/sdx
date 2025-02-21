#include "sched/sched.h"
#include "sched/signal.h"
#include "sched/stack.h"

#include "util/string.h"
#include "util/list.h"
#include "util/mem.h"

#include "mm/vmm.h"
#include "mm/heap.h"

#include "types.h"
#include "errno.h"

task_t *task_new() {
  task_t *task_new = heap_alloc(sizeof(task_t));
  int32_t err      = 0;

  // clear the stack structure
  bzero(task_new, sizeof(task_t));

  // create a new VMM for the task
  if (NULL != task_current)
    task_new->vmm = vmm_new();
  else
    task_new->vmm = vmm_get();

  /*

   * if we are copying from an other process copy it's
   * memory regions, otherwise clear all the regions
   * and allocate a new memory region for the stack

  */
  if (NULL != task_current)
    err = task_mem_copy(task_new, task_current);

  else {
    task_mem_clear(task_new);
    err = task_stack_alloc(task_new);
  }

  // check for errors
  if (err != 0)
    return NULL;

  // setup new task's signal queue
  task_signal_clear(task_new);

  // copy registers
  if (NULL != task_current)
    memcpy(&task_new->regs, &task_current->regs, sizeof(task_regs_t));

  return task_new;
}

int32_t task_rename(task_t *task, const char *name) {
  if (NULL == task || NULL == name)
    return -EINVAL;

  strncpy(task->name, name, NAME_MAX + 1);
  return 0;
}

int32_t task_free(task_t *task) {
  if (NULL == task)
    return -EINVAL;

  task_mem_clear(task);    // free the task's memory regions
  task_signal_clear(task); // free the task's signal queue

  // free the task structure
  heap_free(task);
  return 0;
}

void __task_mem_free(mem_t *mem) {
  mem_unmap(mem);
  mem_free(mem);
}

int32_t task_mem_del(task_t *task, mem_type_t type, uint64_t vma) {
  if (NULL == task)
    return -EINVAL;

  mem_t *mem = NULL;

  if (NULL == mem)
    return -EFAULT;

  vmm_save();
  task_vmm_switch(task);

  while (NULL != (mem = mem_del(&task->mem, type, vma)))
    __task_mem_free(mem);

  vmm_restore();
  return 0;
}

int32_t task_mem_clear(task_t *task) {
  vmm_save();
  task_vmm_switch(task);

  slist_clear(&task->mem, __task_mem_free, mem_t);

  vmm_restore();
  return 0;
}

int32_t task_mem_copy(task_t *to, task_t *from) {
  mem_t  *copy = NULL;
  int32_t err  = 0;

  vmm_save();
  task_vmm_switch(to);

  slist_foreach(&from->mem, mem_t) {
    if ((copy = mem_copy(cur)) == NULL) {
      err = -EFAULT;
      goto end;
    }
    task_mem_add(to, copy);
  }

end:
  vmm_restore();
  return err;
}
