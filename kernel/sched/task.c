#include "sched/sched.h"
#include "sched/task.h"

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

  // clear the task structure
  bzero(task_new, sizeof(task_t));

  // use the current VMM
  sched_debg("using the current VMM for the new task 0x%p", task_new);
  task_new->vmm = vmm_get();

  // allocate a new stack for the new task
  sched_debg("allocating a new stack for the new task 0x%p", task_new);
  if ((err = task_stack_alloc(task_new)) != 0) {
    sched_fail("failed to allocate a new stack for the tasK 0x%p: %s", task_new, strerror(err));
    heap_free(task_new);
    return NULL;
  }

  // return the new task
  return task_new;
}

task_t *task_copy() {
  task_t   *copy = heap_alloc(sizeof(task_t));
  region_t *cur = NULL, *new = NULL;
  int32_t   err = 0;

  // clear the stack structure
  bzero(copy, sizeof(task_t));

  // create a new VMM for the task
  sched_debg("creating a new VMM for the task 0x%p", task_new);
  copy->vmm = vmm_new();

  // copy the task's memory regions
  for (cur = current->mem; cur != NULL; cur = cur->next) {
    // copy the memory region
    if ((new = region_copy(cur)) == NULL) {
      sched_fail("failed to copy the %s memory region (0x%p)", region_name(cur), cur->vaddr);
      return NULL;
    }

    // add new memory region to the task
    sched_debg("adding %s memory region @ 0x%p (%u pages)", region_name(cur), cur->vaddr, cur->num);
    task_mem_add(copy, new);
  }

  // copy the registers
  sched_debg("copying registers from current task");
  memcpy(&copy->regs, &task_current->regs, sizeof(task_regs_t));

  // return the copied task
  return copy;
}

void task_free(task_t *task) {
  sched_debg("freeing the task 0x%p", task);

  // free the memory regions
  region_each(&task->mem) {
    sched_debg("freeing %s memory region @ 0x%p (%u pages)", region_name(cur), cur->paddr, cur->num);
    region_free(cur);
  }

  // clear the signal & wait queue
  task_signal_clear(task);
  task_waitq_clear(task);

  // close all the files
  for (uint8_t fd = 0; fd < CONFIG_TASK_FILES_MAX; fd++) {
    vfs_close(task->files[fd]->node);
    heap_free(task->files[fd]);
  }

  // free the VMM
  vmm_free(task->vmm);

  // free the task structure
  heap_free(task);
}

int32_t task_rename(task_t *task, const char *name) {
  if (NULL == task || NULL == name)
    return -EINVAL;

  strncpy(task->name, name, NAME_MAX);
  return 0;
}

int32_t task_switch(task_t *task) {
  int32_t err = 0;

  if (vmm_get() == task->vmm)
    return 0;

  if (task->old) {
    vmm_sync(task->vmm);
    task->old = false;
  }

  if ((err = vmm_switch(task->vmm)) != 0) {
    sched_fail("failed to switch to the task VMM: %s", strerror(err));
    return err;
  }

  region_each(&task->mem) {
    if ((err = region_map(cur)) != 0) {
      sched_fail("failed to map the %s memory region (0x%p)", region_name(cur), cur->vaddr);
      return err;
    }
  }

  return 0;
}
