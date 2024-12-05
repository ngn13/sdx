.intel_syntax noprefix

#include "util/stack.S"

.section .text
.code64

.global _panic

/*

 * panic is using assembly instead of C, because we need to save the registers
 * and the stack for an accurate register and stack dump

 * otherwise all the registers and shit would be overwritten by the function

*/

_panic:
  push rbp // lets make sure to not break the stack trace

  push rdi // save first arg (function name)
  push rsi // save second arg (panic message)
  push [rsp+(8*3)] // save the return address

  // rdi will store the stack frame end for the caller
  mov rdi, rsp
  sub rdi, (8*15) // get space for all 16 registers
  
  push_all // save all the registers

  call __panic

  // this part doesn't really matter because panic won't return anyway
  pop_all // restore all the registers

  pop rsi
  pop rdi
  pop rbp

  ret
