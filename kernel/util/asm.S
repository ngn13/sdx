#define ASM_FILE 1
#include "util/asm.h"

.type _get_cr0, @function
.type _get_cr2, @function
.type _get_cr3, @function
.type _get_cr4, @function
.type _hang, @function

.global _get_cr0
.global _get_cr2
.global _get_cr3
.global _get_cr4
.global _hang

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
