#include "sched/sched.h"
#include "sched/stack.h"
#include "sched/task.h"
#include "sched/mem.h"

#include "core/user.h"
#include "boot/gdt.h"

#include "mm/vmm.h"
#include "mm/pm.h"

#include "fs/fmt.h"
#include "fs/vfs.h"

#include "util/asm.h"
#include "util/panic.h"

#include "types.h"
#include "errno.h"

#define user_debg(f, ...) pdebg("User: (%s:%d:%s) " f, current->name, current->pid, __func__, ##__VA_ARGS__)
#define user_info(f, ...) pinfo("User: (%s:%d:%s) " f, current->name, current->pid, __func__, ##__VA_ARGS__)
#define user_fail(f, ...) pfail("User: (%s:%d:%s) " f, current->name, current->pid, __func__, ##__VA_ARGS__)

#define MSR_EFER  0xC0000080 // EFER (Extended Feature Enables) MSR
#define MSR_STAR  0xC0000081 // STAR (System Call Target Address) MSR
#define MSR_LSTAR 0xC0000082 // LSTAR (IA-32e Mode System Call Target Address) MSR
#define MSR_FMASK 0xC0000084 // FMASK (System Call Flag Mask) MSR

struct user_call user_calls[] = {
    {.code = 0, .func = user_exit},
    {.code = 1, .func = user_fork},
    {.code = 2, .func = user_exec},
    {.func = NULL},
};

int32_t user_setup() {
  /*

   * see Table 2-2. IA-32 Architectural MSRs (Contd.) and
   * see SDM Vol 3, 6.8.8 Fast System Calls in 64-Bit Mode

   * to enable SYSCALL/SYSRET, first thing we need to set bit 0 (SCE)
   * in the EFER MSR, then to get them to actually work, we need to set some other MSRs

   * STAR[47:32] = code segment (CS) for kernel (used for syscall)
   * stack segment (SS) is calculated by STAR[47:32] + 8

   * STAR[63:48] + 16 = code segment (CS) for user (user for sysret)
   * stack segment is calculated by STAR[63:48] + 8... why? fuck you that's why

   * LSTAR stores the 64 bit address for the handler that will be called by syscall
   * and FMASK stores a mask for RFLAGS, so when we call syscall, CPU will AND the
   * FMASK's complement with the current RFLAGS to calculate the new RFLAGS... except the
   * first 32 bits are reserved so its actually calculating the EFLAGS... why? well fuck you

  */
  if (gdt_offset(gdt_desc_code_0_addr) + 8 != gdt_offset(gdt_desc_data_0_addr))
    panic("Invalid GDT structure (bad ring 0 SS offset)");

  if (gdt_offset(gdt_desc_data_3_addr) + 8 != gdt_offset(gdt_desc_code_3_addr))
    panic("Invalid GDT structure (bad ring 3 CS offset)");

  uint64_t efer = _msr_read(MSR_EFER);
  _msr_write(MSR_EFER, efer | 1); // just enable SCE

  _msr_write(MSR_STAR, (gdt_offset(gdt_desc_code_0_addr) << 32) | ((gdt_offset(gdt_desc_data_0_addr) - 5) << 48));
  _msr_write(MSR_LSTAR, (uint64_t)_user_handler);
  _msr_write(MSR_FMASK,
      UINT64_MAX - (1 << 1)); // bit 1 reserved in eflags (see https://en.wikipedia.org/wiki/FLAGS_register#FLAGS)

  return 0;
}

int32_t user_exit(int32_t code) {
  sched_exit(code);
  sched();

  return 0; // will never return
}

int32_t user_exec(char *path, char *argv[], char *envp[]) {
  if (NULL == path)
    return -EINVAL;

  vfs_node_t *node       = vfs_get(path);
  void       *stack_argv = NULL, *stack_envp = NULL;
  void       *fmt_addr = NULL, *fmt_entry = NULL;
  uint64_t    len = 0, fmt_count = 0;
  int32_t     err = 0, i = 0;

  // see if we found the node
  if (NULL == node)
    return -ENOENT;

  // we cannot execute a directory
  if (vfs_node_is_directory(node))
    return -EPERM;

  // TODO: handle shebang

  // try to load the file using a known format
  if ((err = fmt_load(node, &fmt_entry, &fmt_addr, &fmt_count)) < 0) {
    user_fail("failed to load %s: %s", path, strerror(err));
    return err;
  }

  user_debg("entry for the new executable: 0x%x", fmt_entry);

  // make sure the scheduling is disabled before we modify the current task
  sched_lock();
  sched();

  task_update(current, path, TASK_RING_USER, fmt_entry); // update the current task
  task_mem_add(current, fmt_addr, fmt_count);            // add the executable memory region to the task

  // copy the environment variables to the stack
  if ((err = task_stack_copy_list(current, envp, ENV_MAX, &stack_envp)) < 0)
    panic("Exec: failed to copy arguments to new task stack for %s", path);

  // copy the arguments to the stack
  if ((err = task_stack_copy_list(current, argv, ARG_MAX, &stack_argv)) < 0)
    panic("Exec: failed to copy arguments to new task stack for %s", path);

  // add pointers for the argv and envp to the stack
  task_stack_copy(current, &stack_envp, sizeof(void *));
  task_stack_copy(current, &stack_argv, sizeof(void *));

  current->state = TASK_STATE_READY; // mark the current task ready

  user_info("new executable is ready");

  sched_unlock(); // enable scheduling back again after we are done modifying the current task
  sched();        // call the scheduler so next time we switch to this task, we will be running as the new process

  return 0; // will never return
}

int32_t user_fork() {
  // TODO: implement
  return -ENOSYS;
}
