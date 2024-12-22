#include "sched/stack.h"
#include "mm/pm.h"

#include "util/string.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

int32_t task_stack_alloc(task_t *task) {
  /*

   * we have two stacks, one for ring 3 (userland) and one for ring 0 (kernel)
   * we switch between them while switching between rings (syscalls)

  */

  // allocate new kernel & user stack if not already allocated
  if (NULL == task->stack.kernel && (task->stack.kernel = pm_alloc(TASK_STACK_PAGE_COUNT)) == NULL)
    return -ENOMEM;

  if (NULL == task->stack.user && (task->stack.user = pm_alloc(TASK_STACK_PAGE_COUNT)) == NULL)
    return -ENOMEM;

  // disable execution for the stack pages
  pm_set((uint64_t)task->stack.kernel, TASK_STACK_PAGE_COUNT, PM_ENTRY_FLAG_XD);
  pm_set((uint64_t)task->stack.user, TASK_STACK_PAGE_COUNT, PM_ENTRY_FLAG_XD);

  // allow userland access to user stack
  pm_set_all((uint64_t)task->stack.user, TASK_STACK_PAGE_COUNT, PM_ENTRY_FLAG_US);

  return 0;
}

uint64_t task_stack_copy(task_t *task, void *val, uint64_t size) {
  // copy the value to the stack
  task->regs.rsp -= size;
  memcpy((void *)task->regs.rsp, val, size);

  // fix the stack alignment
  for (; task->regs.rsp % 8 != 0; task->regs.rsp--, size++)
    ;

  return size;
}

int32_t task_stack_copy_list(task_t *task, char *list[], uint64_t limit, void **stack) {
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

    task_stack_copy(task, list[i], len);

    *cur = (void *)task->regs.rsp;
    cur++;
  }

  *cur = NULL;
  return 0;
}

void task_stack_free(task_t *task) {
  // free the task's stacks
  pm_free(task->stack.kernel, TASK_STACK_PAGE_COUNT);
  pm_free(task->stack.user, TASK_STACK_PAGE_COUNT);
}

uint64_t task_stack_get(task_t *task, uint8_t ring) {
  if (NULL == task)
    return 0;

  switch (ring) {
  case TASK_RING_KERNEL:
    return (uint64_t)task->stack.kernel;

  case TASK_RING_USER:
    return (uint64_t)task->stack.user;
  }

  return 0;
}
