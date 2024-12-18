#include "core/proc.h"
#include "core/sched.h"

#include "fs/fmt.h"
#include "fs/vfs.h"

#include "mm/vmm.h"
#include "mm/pm.h"

#include "util/mem.h"
#include "util/list.h"
#include "util/panic.h"
#include "util/bit.h"

#include "errno.h"

proc_t *proc_head = NULL;

#define proc_debg(f, ...) pdebg("Proc: (0x%x:%d) " f, proc, NULL == proc ? -1 : proc->pid, ##__VA_ARGS__)
#define proc_info(f, ...) pinfo("Proc: (0x%x:%d) " f, proc, NULL == proc ? -1 : proc->pid, ##__VA_ARGS__)
#define proc_fail(f, ...) pfail("Proc: (0x%x:%d) " f, proc, NULL == proc ? -1 : proc->pid, ##__VA_ARGS__)

#define __proc_add(proc) slist_add(&proc_head, proc, proc_t)
#define __proc_del(proc) slist_del(&proc_head, proc, proc_t)
#define __proc_free(proc)                                                                                              \
  do {                                                                                                                 \
    task_t *pt = proc->task;                                                                                           \
    fmt_free(&proc->fmt);                                                                                              \
    vmm_free(proc);                                                                                                    \
    sched_kill(pt);                                                                                                    \
  } while (0)

proc_t *__proc_new() {
  // allocate a new processes structure
  proc_t *new = vmm_alloc(sizeof(proc_t)), *parent = NULL;

  if (NULL == new)
    return NULL;

  bzero(new, sizeof(proc_t));

  // obtain a PID
  slist_foreach(&proc_head, proc_t) {
    if (cur->pid > new->pid)
      new->pid = cur->pid;
  }

  if (new->pid == UINT32_MAX)
    panic("Run out of PIDs (how did that even happen)");

  new->pid++;

  // obtain the parent PID
  if ((parent = proc_current()) == NULL)
    new->ppid = 0;
  else
    new->ppid = parent->pid;

  return new;
}

void __proc_task_add_to_stack(task_t *task, void *val, uint64_t size) {
  // copy the value to the stack
  task->regs.rsp -= size;
  memcpy((void *)task->regs.rsp, val, size);

  // fix the stack alignment
  for (; task->regs.rsp % 8 != 0; task->regs.rsp--)
    ;
}

int32_t __proc_task_copy_list(task_t *task, char *list[], uint64_t limit, void **listp) {
  /*

   * this function is used to copy the argv and envp to the
   * new process' stack, and here is the layout we use :

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
  cur = *listp = (void *)task->regs.rsp;

  // copy all the elements
  for (i = 0; NULL != list && list[i] != NULL; i++) {
    if ((total += len = strlen(list[i]) + 1) > limit)
      return -E2BIG;

    __proc_task_add_to_stack(task, list[i], len);

    *cur = (void *)task->regs.rsp;
    cur++;
  }

  *cur = NULL;
  return 0;
}

int32_t proc_exec(char *path, char *argv[], char *envp[]) {
  if (NULL == path)
    return -EINVAL;

  vfs_node_t *node = vfs_get(path);
  proc_t     *proc = NULL;

  void    *stack_argv = NULL, *stack_envp = NULL;
  int32_t  err = 0, i = 0;
  uint64_t len = 0;

  // see if we found the node
  if (NULL == node)
    return -ENOENT;

  // we cannot execute a directory
  if (vfs_node_is_directory(node))
    return -EPERM;

  // TODO: handle shebang

  // create a new process
  if ((proc = __proc_new()) == NULL) {
    proc_debg("failed to create a process for %s", path);
    return -ENOMEM;
  }

  // try to load the file using a known format
  if ((err = fmt_load(node, &proc->fmt)) < 0) {
    proc_debg("failed to load %s", path, strerror(err));
    goto ret;
  }

  // make sure the allocated pages for the file are accessable by ring 3
  pm_set_all((uint64_t)proc->fmt.addr, proc->fmt.pages, PM_ENTRY_FLAG_US);

  // create a new task for the process
  if ((proc->task = sched_new(node->name, proc->fmt.entry, TASK_RING_USER)) == NULL) {
    proc_debg("failed to create a new task for %s", path);
    err = -EFAULT;
    goto ret;
  }

  // copy the environment variables to the stack
  if ((err = __proc_task_copy_list(proc->task, envp, ENV_MAX, &stack_envp)) < 0) {
    proc_debg("failed to copy arguments to new task stack for %s", path);
    goto ret;
  }

  // copy the arguments to the stack
  if ((err = __proc_task_copy_list(proc->task, argv, ARG_MAX, &stack_argv)) < 0) {
    proc_debg("failed to copy arguments to new task stack for %s", path);
    goto ret;
  }

  // add pointers for the argv and envp to the stack
  __proc_task_add_to_stack(proc->task, &stack_envp, sizeof(void *));
  __proc_task_add_to_stack(proc->task, &stack_argv, sizeof(void *));

ret:
  // something went wrong, free the process
  if (err < 0) {
    __proc_free(proc);
    return err;
  }

  // process (so the task) is ready, add it to the list
  __proc_add(proc);
  sched_ready(proc->task);

  // return the process ID
  proc_debg("created a new process (PID: %d Task: %s)", proc->pid, proc->task->name);
  return proc->pid;
}

void __proc_kill(proc_t *proc) {
  if (proc == proc_head)
    panic_nd("Attempted to kill init");

  bool reached_end = false;

  while (!reached_end) {
    slist_foreach(&proc_head, proc_t) {
      reached_end = slist_is_end();

      if (cur->ppid == proc->pid) {
        __proc_kill(cur);
        break;
      }
    }
  }

  __proc_del(proc);
  __proc_free(proc);
}

int32_t proc_kill(pid_t pid) {
  proc_t *proc = proc_find(pid);

  if (NULL == proc)
    return -ESRCH;

  __proc_kill(proc);
  return 0;
}

proc_t *proc_find(pid_t pid) {
  slist_foreach(&proc_head, proc_t) {
    if (cur->pid == pid)
      return cur;
  }

  return NULL;
}

proc_t *proc_next(proc_t *proc) {
  return NULL == proc ? proc_head : proc->next;
}

proc_t *proc_current() {
  slist_foreach(&proc_head, proc_t) {
    if (current == cur->task)
      return cur;
  }

  return NULL;
}

void __proc_exception_handler(im_stack_t *stack) {
  proc_t *proc = proc_current();

  // not running as a process
  if (NULL == proc)
    return;

  switch (stack->vector) {
  case IM_INT_DIV_ERR:
    proc_fail("received an division by zero exception");
    break;

  case IM_INT_INV_OPCODE:
    proc_fail("received an invalid opcode exception");
    break;

  case IM_INT_DOUBLE_FAULT:
    proc_fail("received a double fault exception");
    break;

  case IM_INT_GENERAL_PROTECTION_FAULT:
    proc_fail("received a general protection fault exception");
    break;

  case IM_INT_PAGE_FAULT:
    proc_fail("received an page fault exception");
    printf("      P=%u W=%u U=%u R=%u I=%u PK=%u SS=%u SGX=%u\n",
        bit_get(stack->error, 0),
        bit_get(stack->error, 1),
        bit_get(stack->error, 2),
        bit_get(stack->error, 3),
        bit_get(stack->error, 4),
        bit_get(stack->error, 5),
        bit_get(stack->error, 6),
        bit_get(stack->error, 7));
    break;

  default:
    proc_fail("received an unknown exception (0x%x)", stack->vector);
    break;
  }

  proc_fail("Dumping process core");
  core_dump(&proc->task->regs);

  // kill the process so the sched doesn't panic
  __proc_kill(proc);
}

void proc_init() {
  // make sure the list is empty
  proc_head = NULL;

  // add the exception handler
  for (uint8_t i = 0; i < IM_INT_EXCEPTIONS; i++)
    im_add_handler(i, IM_HANDLER_PRIO_SECOND, __proc_exception_handler);

  // attempt to execute init
  if (proc_exec("/init", NULL, NULL) < 0)
    panic("Failed to execute init");
}
