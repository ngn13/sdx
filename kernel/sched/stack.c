#include "sched/stack.h"
#include "sched/task.h"
#include "sched/mem.h"

#include "util/list.h"
#include "util/string.h"
#include "util/mem.h"

#include "mm/vmm.h"

#include "errno.h"
#include "types.h"

int32_t task_stack_alloc(task_t *task) {
  vmm_save();
  task_vmm_switch(task);

  /*

   * we have two stacks, one for ring 3 (userland) and one for ring 0 (kernel)
   * we switch between them while switching between rings (syscalls)

   * this function allocates both of these stacks and adds them to the memory
   * region list of the task

  */
  void *kernel_stack = vmm_map_with(TASK_STACK_PAGE_COUNT, 0, VMM_VMA_KERNEL, VMM_FLAGS_DEFAULT | VMM_FLAG_XD);
  void *user_stack   = vmm_map_with(TASK_STACK_PAGE_COUNT, 0, VMM_VMA_USER, VMM_FLAGS_DEFAULT | VMM_FLAG_XD);

  if (NULL == kernel_stack || NULL == user_stack)
    return -ENOMEM;

  task_mem_add(task, TASK_MEM_TYPE_STACK, kernel_stack, vmm_resolve(kernel_stack), TASK_STACK_PAGE_COUNT);
  task_mem_add(task, TASK_MEM_TYPE_STACK, user_stack, vmm_resolve(user_stack), TASK_STACK_PAGE_COUNT);

  vmm_restore();
  return 0;
}

uint64_t __task_stack_add_internal(task_t *task, void *val, uint64_t size) {
  // copy the value to the stack
  task->regs.rsp -= size;
  memcpy((void *)task->regs.rsp, val, size);

  // fix the stack alignment
  for (; task->regs.rsp % 8 != 0; task->regs.rsp--, size++)
    ;

  return size;
}

uint64_t task_stack_add(task_t *task, void *val, uint64_t size) {
  vmm_save();
  task_vmm_switch(task);

  uint64_t ret = __task_stack_add_internal(task, val, size);

  // restore the old VMM
  vmm_restore();
  return ret;
}

int32_t task_stack_add_list(task_t *task, char *list[], uint64_t limit, void **stack) {
  /*

   * this function is used to copy the argv and envp to the
   * new process' stack, and here is the layout we use:

   * --- higher address ---
   * NULL
   * arg/env 4 pointer
   * arg/env 3 pointer ----.
   * arg/env 2 pointer     |
   * arg/env 1 pointer <-. |
   * arg/env 4 value     | |
   * arg/env 3 value <---|-'
   * arg/env 2 value     |
   * arg/env 1 value     |
   * ...............     |
   * argv/envp pointer --'

  */

  vmm_save();
  task_vmm_switch(task);

  uint64_t len = 0, total = 0, i = 0;
  char   **cur = NULL;

  // create space for the pointer list
  for (i = 0; NULL != list && list[i] != NULL; i++) {
    if (i > limit) // each element is at least one byte
      return -E2BIG;
    task->regs.rsp -= sizeof(char *);
  }

  task->regs.rsp -= sizeof(char *); // one more for NULL
  cur = *stack = (void *)task->regs.rsp;

  // copy all the elements
  for (i = 0; NULL != list && list[i] != NULL; i++) {
    if ((total += len = strlen(list[i]) + 1) > limit)
      return -E2BIG;

    __task_stack_add_internal(task, list[i], len);

    *cur = (void *)task->regs.rsp;
    cur++;
  }

  // end pointer list with a NULL pointer
  *cur = NULL;

  vmm_restore();
  return 0;
}

uint64_t task_stack_get(task_t *task, uint64_t vma) {
  slist_foreach(&task->mem, task_mem_t) {
    if (cur->type != TASK_MEM_TYPE_STACK)
      continue;

    if (vma == VMM_VMA_USER && cur->vaddr < (void *)VMM_VMA_USER_END)
      return (uint64_t)cur->vaddr + TASK_STACK_PAGE_COUNT * VMM_PAGE_SIZE;

    if (vma == VMM_VMA_KERNEL && cur->vaddr > (void *)VMM_VMA_USER_END)
      return (uint64_t)cur->vaddr + TASK_STACK_PAGE_COUNT * VMM_PAGE_SIZE;
  }

  return 0;
}
