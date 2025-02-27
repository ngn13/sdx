#include "util/asm.h"
#include "types.h"

.global _end_addr
.global _start_addr

.global _get_cr0
.global _get_cr2
.global _get_cr3
.global _get_cr4

.global _msr_read
.global _msr_write

.global _hang

.type _end_addr,   @common
.type _start_addr, @common

.type _get_cr0, @function
.type _get_cr2, @function
.type _get_cr3, @function
.type _get_cr4, @function

.type _msr_read,  @function
.type _msr_write, @function

.type _hang, @function

.section .data
_start_addr: .quad _start
_end_addr:   .quad _end

.section .text
.code64

_get_cr0:
  mov %cr0, %rax
  ret

_get_cr2:
  mov %cr2, %rax
  ret

_get_cr3:
  mov %cr3, %rax
  ret

_get_cr4:
  mov %cr4, %rax
  ret

_hang:
  hlt
  jmp _hang

_msr_write:
  /*

   * wrmsr only writes value specified in edx:eax
   * to the msr specified in ecx, so lets save those

  */
  push %rcx
  push %rdx
  push %rax

  mov %edi, %ecx // first argument: msr
  mov %rsi, %rax // second argument value

  xor %rdx, %rdx // zero rdx
  shld $32, %rsi, %rdx // rdx now stores high 32 bits of the value

  wrmsr

  pop %rax
  pop %rdx
  pop %rcx

  ret

_msr_read:
  /*

   * similar to the wrmsr, rdmsr only reads value into edx:eax
   * from the msr specified in ecx

  */
  push %rcx
  push %rdx

  mov %edi, %ecx // first argument: msr

  xor %rax, %rax // zero rax
  xor %rdx, %rdx // zero rdx

  rdmsr

  shl $32, %rdx
  or %rdx, %rax

  pop %rdx
  pop %rcx

  ret
