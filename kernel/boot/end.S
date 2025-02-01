.global end_addr
.global __end_get_addr

.type end_addr,       @common
.type __end_get_addr, @function

.section .data
end_addr: .int 0

// this section is placed to end of the binary (see config/linker.ld)
.section .end
.skip 16
_end_addr:

.section .text
.code32

__end_get_addr:
  mov $end_addr, %eax
  mov $_end_addr, (%eax)
  ret
