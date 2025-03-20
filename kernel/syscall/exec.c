#include "syscall.h"
#include "sched/sched.h"

#include "util/string.h"
#include "util/panic.h"
#include "util/mem.h"

#include "fs/fmt.h"
#include "mm/vmm.h"

#include "errno.h"
#include "types.h"

int32_t sys_exec(char *path, char *argv[], char *envp[]) {
  if (NULL == path)
    return -EINVAL;

  sys_debg("executing %s", path);
  sys_debg("argv: 0x%p", argv);
  sys_debg("envp: 0x%p", envp);

  vfs_node_t *node = NULL;
  region_t   *cur  = NULL;
  fmt_t       fmt;

  char  **argv_copy = NULL, **envp_copy = NULL;
  void   *stack_argv = NULL, *stack_envp = NULL;
  int32_t err = 0;

  // try to open the VFS node
  if ((err = vfs_open(&node, path)) != 0)
    goto end;

  // we cannot execute a directory
  if (vfs_node_is_directory(node)) {
    err = -EACCES;
    goto end;
  }

  // TODO: handle shebang

  // copy the arguments
  if (NULL != argv)
    argv_copy = charlist_copy(argv, ARG_MAX);

  // copy the environment vars
  if (NULL != envp)
    envp_copy = charlist_copy(envp, ENV_MAX);

  // try to load the file using a known format
  if ((err = fmt_load(node, &fmt)) < 0) {
    sys_fail("failed to load %s: %s", path, strerror(err));
    goto end;
  }

  sys_debg("entry for the new executable: 0x%x", fmt.entry);

  // close the VFS node
  if ((err = vfs_close(node)) != 0) {
    sys_debg("failed to close the %s VFS node", path);
    goto end;
  }

  /*

   * we are gonna modify the current task, if an IRQ calls
   * or somehow the sched() gets called, its gonna do the
   * scheduling stuff which will mess up our changes, so
   * we put the scheduler on hold until we are done

  */
  sched_hold();

  // update the current task
  task_rename(current, path);

  // remove old binary format memory regions
  while (NULL != (cur = task_mem_find(current, REGION_TYPE_CODE, VMM_VMA_USER)))
    task_mem_del(current, cur);

  while (NULL != (cur = task_mem_find(current, REGION_TYPE_RDONLY, VMM_VMA_USER)))
    task_mem_del(current, cur);

  while (NULL != (cur = task_mem_find(current, REGION_TYPE_DATA, VMM_VMA_USER)))
    task_mem_del(current, cur);

  // add the new regions from the loaded format
  task_mem_add(current, fmt.mem);

  // update the registers
  bzero(&current->regs, sizeof(task_regs_t));

  /*

   * bit 1 = reserved, 9 = interrupt enable
   * https://en.wikipedia.org/wiki/FLAGS_register

  */
  current->regs.rflags = ((1 << 1) | (1 << 9));
  current->regs.rip    = (uint64_t)fmt.entry;

  /*

   * old code for jumping to kernel code

   * task_current->regs.cs  = gdt_offset(gdt_desc_kernel_code_addr);
   * task_current->regs.ss  = gdt_offset(gdt_desc_kernel_data_addr);
   * task_current->regs.rsp = task_stack_get(task_current, VMM_VMA_KERNEL);

  */

  current->regs.cs  = gdt_offset(gdt_desc_user_code_addr);
  current->regs.ss  = gdt_offset(gdt_desc_user_data_addr);
  current->regs.rsp = (uint64_t)task_stack_get(task_current, VMM_VMA_USER);

  /*

   * ORed with 3 to set the RPL to 3
   * see https://wiki.osdev.org/Segment_Selector

  */
  current->regs.cs |= 3;
  current->regs.ss |= 3;

  // copy the environment variables to the stack
  if ((err = task_stack_add_list(current, envp_copy, ENV_MAX, &stack_envp)) != 0)
    panic("Failed to copy arguments to new task stack for %s", path);

  // copy the arguments to the stack
  if (NULL != argv_copy)
    err = task_stack_add_list(current, argv_copy, ARG_MAX, &stack_argv);

  // don't allow NULL argv
  else {
    char *temp_argv[] = {task_current->name, NULL};
    err               = task_stack_add_list(current, temp_argv, ARG_MAX, &stack_argv);
  }

  if (err != 0)
    panic("Failed to copy arguments to new task stack for %s", path);

  // add pointers for the argv and envp to the stack
  task_stack_add(current, &stack_envp, sizeof(void *));
  task_stack_add(current, &stack_argv, sizeof(void *));

  // call the scheduler to run as the new task
  sys_info("executing the new binary");

end:
  // free the copy of the argument and the environment list
  charlist_free(argv_copy);
  charlist_free(envp_copy);

  /*

   * our modifications are complete, reset the priority of the task
   * and unhold the scheduler which will put us on SAVE state to apply
   * our modifications with the next sched()

  */
  sched_prio(TASK_PRIO_LOW);
  sched_done();

  // if everything went fine, will never return
  sched();

  // return the error
  return err;
}
