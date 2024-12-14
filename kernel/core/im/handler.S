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

 * this macro is used to create unique handlers for every interrupt
 * it saves the interrupt vector and the registers to the stack

*/
.macro __im_handle_n n
.global __im_handle_\n
__im_handle_\n:
  push_all // save all the registers (temporary)

  /*

   * see if we have the error code on the stack,
   * if not, then push a fake error code

   * the reason why we are doing this is that only some
   * interrupt vectors push an error code (see the list at the start)

   * to do this we loop over the list of interrupt vectors
   * that push an error code to the stack:
   * - rbx = stores the pointer for __int_err_codes
   * - rcx = stores the current char

  */
  lea rbx, __int_err_codes

  .Lim_handle_check_err_codes_\n:
    // get the next vector on the list
    xor rcx, rcx
    mov cl, byte ptr [rbx+0]
    inc rbx

    // check if we reached the end of the list (0x0)
    test cl, cl
    jmp .Lim_handle_no_err_\n

    // see if the current vector matches our vector
    cmp cl, \n
    jne .Lim_handle_check_err_codes_\n

.Lim_handle_has_err_\n:
  pop_all
  push \n  // save the interrupt vector
  push_all // save all the registers
  jmp __im_handle

.Lim_handle_no_err_\n:
  pop_all
  push 0 // fake error
  push \n // save the interrupt vector
  push_all // save all the registers
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
.global __im_load

// see core/im/im.c
.global im_idtr

/*

 * interrupt handler wrapper

 * this function calls the actual handler with by passing rsp as a argument
 * and at this point stack has the following structure:
 * - all the registers
 * - interrupt vector
 * - interrupt error code
 * - iret frame (rip, cs, rflags, rsp, ss)

 * which is interpereted as a pointer to im_stack_t structure by the acutal handler
 * and it's used to keep track of the interrupt status

 * this wrapper function also makes sure that the iret frame remains on the top
 * before calling iretq to return back

*/
__im_handle:
  mov rdi, rsp
  call im_handle

  pop_all // restore all the registers
  add rsp, 16 // remove the error code and the vector
  iretq

// load IDTR and TSS
__im_load:
  lidt im_idtr
  mov rax, gdt_desc_tss_addr
  sub rax, gdt_start_addr
  ltr ax
  ret
