.global __paging_setup
.type __paging_setup, @function

.section .text
.code32

__paging_setup:
  push %ebp
  mov %esp, %ebp

  // move the multiboot info address (first arg) to eax
  mov 8(%ebp), %eax

  // get available memory info from multiboot info
  push %eax
  call __mb_get_mem

  // TODO: acutally setup paging

  mov %ebp, %esp
  pop %ebp

  ret
