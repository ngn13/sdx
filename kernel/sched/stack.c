#include "sched/sched.h"
#include "sched/task.h"

#include "mm/region.h"
#include "mm/paging.h"
#include "mm/vmm.h"

#include "util/string.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

int32_t task_stack_alloc(task_t *task) {
  /*

   * we have two stacks, one for ring 3 (userland) and one for ring 0 (kernel)
   * we switch between them while switching between rings (syscalls)

   * this function allocates both of these stacks and adds them to the memory
   * region list of the task

  */
  region_t *kernel_stack = region_new(REGION_TYPE_STACK, VMM_VMA_KERNEL, NULL, TASK_STACK_PAGE_COUNT);
  region_t *user_stack   = region_new(REGION_TYPE_STACK, VMM_VMA_USER, NULL, TASK_STACK_PAGE_COUNT);
  int32_t   err          = 0;

  if ((err = region_map(kernel_stack)) != 0) {
    sched_fail("failed to map kernel stack region for 0x%p: %s", task, strerror(err));
    return err;
  }

  if ((err = region_map(user_stack)) != 0) {
    sched_fail("failed to map user stack region for 0x%p: %s", task, strerror(err));
    return err;
  }

  task_mem_add(task, kernel_stack);
  task_mem_add(task, user_stack);
  return 0;
}

uint64_t task_stack_add(task_t *task, void *val, uint64_t size) {
  // copy the value to the stack
  task->regs.rsp -= size;
  memcpy((void *)task->regs.rsp, val, size);

  // fix the stack alignment
  for (; task->regs.rsp % 8 != 0; task->regs.rsp--, size++)
    ;

  return size;
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

    task_stack_add(task, list[i], len);

    *cur = (void *)task->regs.rsp;
    cur++;
  }

  // end pointer list with a NULL pointer
  *cur = NULL;
  return 0;
}

void *task_stack_get(task_t *task, uint8_t vma) {
  region_t *stack = task_mem_find(task, REGION_TYPE_STACK, vma);

  if (NULL == stack)
    return NULL;

  return stack->vaddr + stack->num * PAGE_SIZE;
}
