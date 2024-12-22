#include "sched/sched.h"
#include "sched/task.h"

#include "util/stack.S"

.section .data
_panic_local_regs_start:
  .skip (TASK_REG_COUNT*8)
_panic_local_regs_end:

.global _panic_local_regs_start
.global _panic_local_regs_end

.section .text
.code64

.type __panic_with, @function // see util/panic.c
.type _panic_with, @function

.extern __panic_with
.global _panic_with

/*

 * panic_with is using assembly, because if task_regs_t is not specified
 * we need to obtain it ourselfs, ofc without modifiying the registers,
 * which is not possible using C

*/

_panic_with:
  // save all the registers (in order, see task_regs_t in inc/core/sched.h)
  push %rsp
  pushfq
  push $0 // cs
  push $0 // ss
  push $0 // rip
  push_all

  // fix the rsp (make sure it points to the callers stack frame)
  addq $8, (TASK_REG_COUNT-1) *8(%rsp)

  // restore the stack
  add $(TASK_REG_COUNT*8), %rsp

  // copy all the pushed registers to _panic_local_regs
  movq %rsp, %r15
  movq $_panic_local_regs_end, %r14

  .Lpanic_with_copy_reg:
    sub $8, %r15
    sub $8, %r14

    movq (%r15), %r13
    movq %r13, (%r14)

    cmpq $_panic_local_regs_start, %r14
    jne .Lpanic_with_copy_reg

  // if register structure is not specified, use _panic_local_regs
  mov $_panic_local_regs_start, %r15
  test %rdi, %rdi
  cmove %r15, %rdi

  call __panic_with

  // if somehow return, just halt
  .Lpanic_with_halt:
    cli
    hlt
    jmp .Lpanic_with_halt
