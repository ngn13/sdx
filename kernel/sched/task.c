#include "sched/sched.h"
#include "sched/task.h"
#include "sched/signal.h"
#include "sched/stack.h"
#include "sched/mem.h"

#include "mm/vmm.h"
#include "mm/pm.h"

#include "boot/gdt.h"

#include "util/string.h"
#include "util/list.h"
#include "util/mem.h"

#include "types.h"
#include "errno.h"

task_t *task_alloc() {
  task_t *new = vmm_alloc(sizeof(task_t));
  bzero(new, sizeof(task_t));
  return new;
}

int32_t task_free(task_t *task) {
  if (NULL == task)
    return -EINVAL;

  task_mem_clear(task);    // free the task's memory regions
  task_signal_clear(task); // free the task's signal queue
  task_stack_free(task);   // free the task's stack

  // free the task structure
  vmm_free(task);

  return 0;
}

int32_t task_update(task_t *task, const char *name, uint8_t ring, void *entry) {
  if (NULL == task || NULL == name || !__task_is_valid_ring(ring))
    return -EINVAL;

  task_stack_alloc(task);  // allocate a user & kernel stack
  task_signal_clear(task); // free the task's signal queue
  task_mem_clear(task);    // free the task's memory regions

  // set the required values
  strncpy(task->name, name, NAME_MAX + 1);
  task->state     = TASK_STATE_BUSY;
  task->prio      = TASK_PRIO_DEFAULT;
  task->min_ticks = TASK_TICKS_DEFAULT;
  task->ring      = ring;

  // update the registers
  bzero(&task->regs, sizeof(task_regs_t));

  /*

   * bit 1 = reserved, 9 = interrupt enable
   * https://en.wikipedia.org/wiki/FLAGS_register

  */
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

    /*

     * ORed with 3 to set the RPL to 3
     * see https://wiki.osdev.org/Segment_Selector

    */
    task->regs.cs |= 3;
    task->regs.ss |= 3;

    break;
  }

  return 0;
}

task_t *task_copy(task_t *task) {
  // TODO: implement
  return NULL;
}
