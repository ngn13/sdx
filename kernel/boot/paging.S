.global __paging_setup
.type __paging_setup, @function

.section .bss
.align 0x1000
tables:
  .fill 512*3,8,0

.section .text
.code32

__paging_setup:
  push %ebp
  mov %esp, %ebp

  // save registers
  push %ebx
  push %edi
  push %ecx

  // setup page table addresses
  mov $tables, %eax  // eax = PML4
  mov %eax, %ebx
  add $(512*8), %ebx // ebx = PDPT
  mov %ebx, %ecx
  add $(512*8), %ecx // ecx = PD

  /*

   * setup PML4 (level 4 page table)
   * map first and last 512 GiBs

  */
  mov %ebx, (%eax)
  add $3, (%eax)
  add $(511*8), %eax
  mov %ebx, (%eax)
  add $3, (%eax)

  /*

   * setup PDPT (level 3 page table)
   * map first 1 GiB for the first 512 GiBs
   * and map 480th GiB for the last 512 GiBs

   * so map first 1 GiB of:
   * - 0x0000000000000000 (lower half start)
   * - 0xfffffff800000000 (upper half start)

  */
  mov %ecx, (%ebx)
  add $3, (%ebx)
  add $(480*8), %ebx
  mov %ecx, (%ebx)
  add $3, (%ebx)

  /*

   * setup PD (level 2 page table)
   * map first 4 MiBs of lower and upper half to 0x0
   * so we are id mapping the lower half

 */
  mov $0x0000000000000083, (%ecx)
  mov $0x0000000000200083, 8(%ecx)

  // return PML4 address
  mov $tables, %eax

  // restore registers
  pop %ecx
  pop %edi
  pop %ebx

  mov %ebp, %esp
  pop %ebp

  ret
