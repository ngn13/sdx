.intel_syntax noprefix
#include "util/stack.S"

.section .data
__int_err_codes:
  /*

   * list of vectors that push an error code, 0 marks the end
   * see https://wiki.osdev.org/Exceptions for more info

  */
  .byte 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0x11, 0x15, 0x1D, 0x1E, 0x0

.section .text
.code64

/*

 * this macro is used to create unique handlers for every macro
 * so we can save and latter on process the interrupt vector

*/
.macro __im_handle_n n
.global __im_handle_\n
__im_handle_\n:
  push_all

  // save the interrupt vector to rsi (first argument)
  mov rdi, \n

  // save the stack start address to rsi (second argument)
  mov rsi, rsp

  jmp __im_handle
.endm

// interrupt handlers
.altmacro
  .set i,0
  .rept 256
  __im_handle_n %i
  .set i,i+1
.endr

.type __im_handle, @function
.type __im_load_idtr, @function

.global __im_handle
.global __im_load_idtr

// see core/im/im.c
.global im_idtr

/*

 * interrupt handler wrapper

 * called by unique interrupt handlers, this function make sures the stack is setup
 * correctly by removing the error code that is pushed by some exception interrupts

 * after making sure the stack is setup correctly, it calls the actual interrupt handler
 * which is defined in core/im.c

 * this function is called with a single argument, which is the interrupt vector, and it's
 * set by the unique interrupt handlers

*/
__im_handle:
  xor rax, rax
  lea rbx, __int_err_codes
  xor rcx, rcx

  /*

   * we are gonna loop over all the interrupt vectors that push an error code
   * and we are gonna pop it if the current vector code pushed an error code:

   * and currently the register status is:
   * - rax = used to save the rbp
   * - rbx = stores the pointer for __int_err_codes
   * - rcx = stores the current char

  */
  .Lim_handle_check_err_codes:
    xor rcx, rcx
    mov cl, byte ptr [rbx+0]

    add rbx, 1

    test cl, cl
    je .Lim_handle_check_err_end

    cmp rcx, rdi
    jne .Lim_handle_check_err_codes

    mov rax, rsp
    add rsp, 15*8
    pop rdx
    mov rsp, rax

  .Lim_handle_check_err_end:
    call im_handle

  pop_all
  iretq

// load IDTR
__im_load_idtr:
  lidt im_idtr
  ret
