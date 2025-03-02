#include "sched/sched.h"
#include "sched/task.h"

#include "core/user.h"
#include "boot/boot.h"

#include "fs/fmt.h"
#include "fs/vfs.h"

#include "mm/region.h"
#include "mm/vmm.h"

#include "util/string.h"
#include "util/panic.h"
#include "util/asm.h"
#include "util/mem.h"

#include "limits.h"
#include "types.h"
#include "errno.h"

#define user_debg(f, ...) pdebg("User: (%d:%s) " f, current->pid, __func__, ##__VA_ARGS__)
#define user_info(f, ...) pinfo("User: (%d:%s) " f, current->pid, __func__, ##__VA_ARGS__)
#define user_fail(f, ...) pfail("User: (%d:%s) " f, current->pid, __func__, ##__VA_ARGS__)

struct user_call user_calls[] = {
    {.code = 0, .func = user_exit},
    {.code = 1, .func = user_fork},
    {.code = 2, .func = user_exec},
    {.code = 3, .func = user_wait},
    {.code = 4, .func = user_open},
    {.code = 5, .func = user_close},
    {.code = 6, .func = user_read},
    {.code = 7, .func = user_write},
    {.func = NULL},
};

int32_t user_setup() {
  /*

   * see Table 2-2. IA-32 Architectural MSRs (Contd.) and
   * see SDM Vol 3, 6.8.8 Fast System Calls in 64-Bit Mode

   * to enable SYSCALL/SYSRET, first thing we need to set bit 0 (SCE) in the EFER MSR,
   * then to get them to actually work, we need to set some other MSRs

   * STAR[47:32] = code segment (CS) for kernel (used for syscall)
   * stack segment (SS) is calculated by STAR[47:32] + 8

   * STAR[63:48] + 16 = code segment (CS) for user (used for sysret)
   * stack segment is calculated by STAR[63:48] + 8... why? fuck you that's why

   * LSTAR stores the 64 bit address for the handler that will be called by syscall
   * and FMASK stores a mask for RFLAGS, so when we call syscall, CPU will AND the
   * FMASK's complement with the current RFLAGS to calculate the new RFLAGS... except the
   * first 32 bits are reserved so its actually calculating the EFLAGS... why? well fuck you

  */
  if (gdt_offset(gdt_desc_kernel_code_addr) + 8 != gdt_offset(gdt_desc_kernel_data_addr))
    panic("Invalid GDT structure (bad ring 0 SS offset)");

  if (gdt_offset(gdt_desc_user_data_addr) + 8 != gdt_offset(gdt_desc_user_code_addr))
    panic("Invalid GDT structure (bad ring 3 CS offset)");

  uint64_t efer = _msr_read(MSR_EFER);
  _msr_write(MSR_EFER, efer | 1); // just enable SCE

  _msr_write(
      MSR_STAR, (gdt_offset(gdt_desc_kernel_code_addr) << 32) | ((gdt_offset(gdt_desc_user_data_addr) - 5) << 48));
  _msr_write(MSR_LSTAR, (uint64_t)_user_handler);
  _msr_write(MSR_FMASK,
      UINT64_MAX - (1 << 1)); // bit 1 reserved in eflags (see https://en.wikipedia.org/wiki/FLAGS_register#FLAGS)

  return 0;
}

void user_exit(int32_t code) {
  user_debg("exiting with code: %d", code);
  sched_exit(code);
  sched(); // will never return
}

int32_t user_exec(char *path, char *argv[], char *envp[]) {
  if (NULL == path)
    return -EINVAL;

  user_debg("executing %s", path);
  user_debg("argv: 0x%p", argv);
  user_debg("envp: 0x%p", envp);

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
    user_fail("failed to load %s: %s", path, strerror(err));
    goto end;
  }

  user_debg("entry for the new executable: 0x%x", fmt.entry);

  // close the VFS node
  if ((err = vfs_close(node)) != 0) {
    user_debg("failed to close the %s VFS node", path);
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
  user_info("executing the new binary");

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

pid_t user_fork() {
  user_debg("forking the current task");

  /*

   * save the caller PID to check if we are running
   * as the parent or the child after the fork

  */
  pid_t caller = current->pid;

  /*

   * setting our TASK_STATE_FORK will make it so the next
   * time the scheduler is called the current task will be
   * forked, to fork the task right now, we call the
   * scheduler ourselves

  */
  sched_state(TASK_STATE_FORK);
  sched();

  // parent returns child PID
  if (caller == current->pid)
    return task_current->cpid;

  // child returns 0
  return 0;
}

pid_t user_wait(int32_t *status) {
  task_waitq_t *waitq = NULL;
  pid_t         pid   = 0;

  // if the wait queue is not empty, just use the next waitq
  if (!task_waitq_is_empty(task_current))
    goto end;

  /*

   * check if we have any children before waiting on
   * a wait queue update, since if we don't have any
   * children task's wait queue will never be updated

  */
  if (NULL == sched_child(task_current, NULL))
    return -ECHILD;

  // wait for a waitq
  while (task_waitq_is_empty(task_current)) {
    sched_state(TASK_STATE_WAIT);
    sched();
  }

end:
  // get the current waitq in the queue
  waitq = task_waitq_pop(task_current);

  // get the waitq status and PID
  *status = waitq->status;
  pid     = waitq->pid;

  // free the waitq
  task_waitq_free(waitq);

  // return the PID
  return pid;
}

int32_t user_open(char *path, int32_t flags, mode_t mode) {
  vfs_node_t  *node = NULL;
  task_file_t *file = NULL;
  int32_t      fd   = -1;

  // try to obtain the node at the path
  if ((fd = vfs_open(&node, path)) != 0)
    goto end;

  // TODO: check permissions (mode)

  // get the next available file descriptor
  if ((fd = task_file_fd_next(task_current)) < 0)
    goto end;

  // update the last file descriptor
  if (fd > task_current->fd_last)
    task_current->fd_last = fd;

  // create a new file object
  if (NULL == (file = heap_alloc(sizeof(task_file_t)))) {
    fd = -ENOMEM;
    goto end;
  }

  // setup the file object
  bzero(file, sizeof(task_file_t));
  file->node  = node;
  file->flags = flags;

  // update the file pointer at the file descriptor index
  task_current->files[fd] = file;

end:
  if (fd < 0) {
    vfs_close(node);
    heap_free(file);
  }

  return fd;
}

int32_t user_close(int32_t fd) {
  // obtain the file object at the given fd
  task_file_t *file = task_file_from(task_current, fd);
  int32_t      err  = 0;

  // check if the fd indexes to an acutal file object
  if (NULL == file)
    return -EBADF;

  // close & free the file
  if ((err = task_file_free(file, false)) != 0)
    return err;

  // update the last file descriptor
  if (fd == task_current->fd_last)
    task_current->fd_last--;

  // remove the file reference from the file list
  task_current->files[fd] = NULL;

  user_debg("closed the file %d", fd);
  return 0;
}

int64_t user_read(int32_t fd, void *buf, uint64_t size) {
  task_file_t *file = task_file_from(task_current, fd);
  int64_t      ret  = 0;

  // check the file obtained with the fd
  if (NULL == file)
    return -EBADF;

  // TODO: check file->flags to make sure we can read

  // perform the read operation
  ret = vfs_read(file->node, file->offset, size, buf);

  if (ret > 0) {
    if (vfs_node_is_directory(file->node))
      // move onto the next directory entry
      file->offset++;
    else
      // increase the offset by read bytes
      file->offset += ret;
  }

  // return the result
  return ret;
}

int64_t user_write(int32_t fd, void *buf, uint64_t size) {
  task_file_t *file = task_file_from(task_current, fd);
  int64_t      ret  = 0;

  // check the file obtained with the fd
  if (NULL == file)
    return -EBADF;

  // TODO: check file->flags to make sure we can write

  // perform the read operation
  ret = vfs_write(file->node, file->offset, size, buf);

  // increase the offset by wroten bytes
  if (ret > 0)
    file->offset += ret;

  // return the result
  return ret;
}
