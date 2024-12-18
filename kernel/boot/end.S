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
  pushl %eax

  mov $end_addr, %eax
  mov $_end_addr, (%eax)

  pop %eax
  ret
