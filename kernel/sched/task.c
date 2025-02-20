#include "sched/task.h"
#include "sched/signal.h"
#include "sched/stack.h"
#include "sched/mem.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/list.h"
#include "util/mem.h"

#include "mm/vmm.h"
#include "mm/heap.h"

#include "types.h"
#include "errno.h"

task_t *task_create(task_t *copy) {

  // TODO: maybe rename this function to task_new() and use current instead of copy?

  task_t *task_new = heap_alloc(sizeof(task_t));
  int32_t err      = 0;

  // clear the stack structure
  bzero(task_new, sizeof(task_t));

  // create a new VMM for the task
  if (NULL != copy)
    task_new->vmm = vmm_new();
  else
    task_new->vmm = vmm_get();

  /*

   * if we are copying from an other process copy it's
   * memory regions, otherwise clear all the regions
   * and allocate a new memory region for the stack

  */
  if (NULL != copy)
    err = task_mem_copy(task_new, copy);

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
  if (NULL != copy)
    memcpy(&task_new->regs, &copy->regs, sizeof(task_regs_t));

  /*

  task->regs.rflags = ((1 << 1) | (1 << 9));
  task->regs.rip    = (uint64_t)entry; // set the instruction pointer to the given entry address

  switch (task->ring) {
  case TASK_RING_KERNEL:
    task->regs.cs  = gdt_offset(gdt_desc_kernel_code_addr);
    task->regs.ss  = gdt_offset(gdt_desc_kernel_data_addr);
    task->regs.rsp = (uint64_t)task->stack.kernel + pm_size(TASK_STACK_PAGE_COUNT);
    break;

  case TASK_RING_USER:
    task->regs.cs  = gdt_offset(gdt_desc_user_code_addr);
    task->regs.ss  = gdt_offset(gdt_desc_user_data_addr);
    task->regs.rsp = (uint64_t)task->stack.user + pm_size(TASK_STACK_PAGE_COUNT);

     * ORed with 3 to set the RPL to 3
     * see https://wiki.osdev.org/Segment_Selector

    task->regs.cs |= 3;
    task->regs.ss |= 3;

    break;
  }*/

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
