.intel_syntax noprefix

/*

 * stores the end section, which used to make sure that a region
 * is actually available as free memory before using it

*/

.section .data
  end_addr: .int 0

.section .end
  .skip 20
  _end_addr:

.global end_addr
.global __end_get_addr

.section .text
.code32

__end_get_addr:
  push eax

  lea eax, _end_addr
  mov [end_addr+0], eax

  pop eax
  ret
