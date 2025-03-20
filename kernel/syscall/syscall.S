#include "sched/task.h"
#include "util/stack.S"
#include "mm/vmm.h"

.section .text
.code64

.type sys_calls,      @object
.type sys_handler,    @function
.type task_stack_get, @function

.global sys_handler
.extern sys_calls // see core/user.c
.extern task_stack_get // see sched/stack.c

sys_handler:
  /*

   * currently we are operating on the user stack
   * and registers are not saved

   * so lets start by saving those

  */
  push_all_save_ret

  // save the syscall number
  mov %rax, %r8

  // also first 4 arguments needs to preserved for the handler call
  mov %rdi, %r12
  mov %rsi, %r11
  mov %rdx, %r10
  mov %rcx, %r9

  // now lets get the kernel stack
  mov task_current, %rdi
  mov $VMM_VMA_KERNEL, %rsi
  call task_stack_get

  // push the current stack position to the new stack
  sub $8, %rax
  mov %rsp, (%rax)

  // switch to the new stack
  mov %rax, %rsp

  // loop through user_calls
  mov $syscalls, %r15

  .Luser_handler_check_call:
    mov $-ENOSYS, %r14
    mov 8(%r15), %r13

    test %r13, %r13
    cmove %r14, %rax
    je .Luser_handler_ret

    mov (%r15), %r14
    add $16, %r15

    cmp %r14, %r8
    jne .Luser_handler_check_call

    // restore argument registers
    mov %r12, %rdi
    mov %r11, %rsi
    mov %r10, %rdx
    mov %r9,  %rcx

    call *%r13

  .Luser_handler_ret:
    pop %rsp // switch back to the old stack
    pop_all_save_ret // restore all the registers
    sysretq // return from the syscall
