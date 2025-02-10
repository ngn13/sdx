#include "boot/boot.h"
#include "mm/vmm.h"

.global paging_mb_data_offset
.global paging_temp_tables_addr
.global __paging_setup
.global __paging_clean

.type paging_mb_data_offset,   @common
.type paging_temp_tables_addr, @common
.type __paging_setup, @function
.type __paging_clean, @function

.section .bss
.align 0x1000
paging_temp_tables:
  .fill 512*4,8,0

.section .data
// error messages
err_paging_kernel_large:  .string "HALTING: Kernel is too large (larger than 4MiB)"
err_paging_mb_data_large: .string "HALTING: Multiboot information is too large (larger than 2MiB)"
paging_mb_data_offset:    .int 0
paging_temp_tables_addr:  .quad paging_temp_tables

.section .text
.code32

/*

 * map first 2 MiBs of the lower half and the -2 GiB mark to kernel binary
 * then map next 4 KiBs to multiboot info and lasty map 510th entry of PML4 to itself
 * for recursive paging

 * this means the kernel has to be <= 2MiBs and multiboot info has to be <= 4KiBs
 * we check both of these cases and call __fail if one fails

 * if successful, our virtual address space will look like this

 * start address     | end address       | size    | desc
 * ------------------|-------------------|---------|------------------------
 * 0x0000000000000000-0x0000000000200000 |   2 MiB | kernel binary
 * 0x0000000000200000-0x0000000000201000 |   4 KiB | multiboot info
 * 0xffffff0000000000-0xffffff8000000000 | 512 GiB | PML4 (recursive paging)
 * 0xffffffff80000000-0xffffffff80200000 |   2 MiB | kernel binary
 * 0xffffffff80200000-0xffffffff80201000 |   4 KiB | multiboot info
 * ------------------|-------------------|---------|------------------------

 * when we jump to higher half, we are gonna call __paging_clean, which
 * will remove the lower half mappings

 * also please note that this is temporary and the actual VMM will most likely
 * end up creating new PML4, PDPT(s), PD(s) and PT(s) for mapping more memory

*/
__paging_setup:
  push %ebp
  mov %esp, %ebp

  // local var to store multiboot info size
  sub $4, %esp

  // save registers
  push %ebx
  push %ecx
  push %edx

  // check kernel size
  mov $(_end - BOOT_KERNEL_ADDR), %eax
  cmpl $(2*1024*1024), %eax
  ja .Lpaging_setup_fail_kernel_large

  // get multiboot info address & size (first 4 bytes)
  mov 8(%ebp), %ebx  // ebx = mb info address
  mov (%ebx), %ecx
  mov %ecx, 12(%esp) // esp+12 = mb info size

  // align address to a 2 MiB page
  .Lpaging_setup_align_mb_addr:
    xor %edx, %edx           // clear remainder
    mov %ebx, %eax           // eax = dividend
    mov $VMM_PAGE_SIZE, %ecx // ecx = divisor
    div %ecx                 // divide to check if we are aligned to a page

    test %edx, %edx // check if the remainder is zero
    je .Lpaging_setup_align_mb_done

    decl %ebx     // decrement the address to align it
    incl 12(%esp) // increment the size

    jmp .Lpaging_setup_align_mb_addr

  .Lpaging_setup_align_mb_done:

  // save the new mb info address
  mov %ebx, %edx

  // calculate the offset
  mov 12(%esp), %ebx // ebx = new size
  mov %ebx, (paging_mb_data_offset - BOOT_KERNEL_ADDR)

  mov 8(%ebp), %ebx // ebx = old address
  mov (%ebx), %ebx  // ebx = old size
  sub %ebx, (paging_mb_data_offset - BOOT_KERNEL_ADDR)

  // check multiboot info size
  mov 12(%esp), %ecx
  cmpl $VMM_PAGE_SIZE, %ecx
  ja .Lpaging_setup_fail_mb_large

  // get page table addresses
  mov $(paging_temp_tables - BOOT_KERNEL_ADDR), %eax  // eax = PML4
  mov %eax, %ebx
  add $(512*8), %ebx // ebx = PDPT
  mov %ebx, %ecx
  add $(512*8), %ecx // ecx = PD

  /*

   * setup PML4 (level 4 page table)
   * map first and last 512 GiBs
   * also we have a resurcive mapping at 511th entry

  */
  mov %ebx, (%eax)
  or $VMM_FLAGS_DEFAULT, (%eax)

  mov %eax, 510*8(%eax)
  or $VMM_FLAGS_DEFAULT, 510*8(%eax)

  mov %ebx, 511*8(%eax)
  or $VMM_FLAGS_DEFAULT, 511*8(%eax)

  /*

   * setup PDPT (level 3 page table)
   * map first 1 GiB for the first 512 GiBs (0x0000000000000000)
   * and map 510th GiB for the last 512 GiBs (0xffffffff80000000)

  */
  mov %ecx, (%ebx)
  or $VMM_FLAGS_DEFAULT, (%ebx)

  mov %ecx, 510*8(%ebx)
  or $VMM_FLAGS_DEFAULT, 510*8(%ebx)

  /*

   * setup PD (level 2 page table)
   * map the kernel binary to first 2 MiBs (located at 0x0)
   * then next entry will point to our PT

  */
  movl $(VMM_FLAGS_DEFAULT | VMM_FLAG_PS), (%ecx)

  add $(512*8*2), %ebx // ebx = PT
  movl %ebx, 8(%ecx)
  or $VMM_FLAGS_DEFAULT, 8(%ecx)

  /*

   * setup PT (level 3 page tabvle)
   * maps first 4KiBs to the multiboot info address we calculated
   * which is stored in edx

  */
  movl %edx, (%ebx)
  or $VMM_FLAGS_DEFAULT, (%ebx)

  // restore registers
  pop %edx
  pop %ecx
  pop %ebx

  mov %ebp, %esp
  pop %ebp

  // eax has PML4's address so we return that
  ret

  .Lpaging_setup_fail_kernel_large:
    push $(err_paging_kernel_large - BOOT_KERNEL_ADDR)
    call __fail

  .Lpaging_setup_fail_mb_large:
    push $(err_paging_mb_data_large - BOOT_KERNEL_ADDR)
    call __fail

.code64
/*

 * remove lower half mappings, at we jumped to the higher half
 * and we are in long mode so we no longer need these mappings

*/
__paging_clean:
  // remove PML4 lower half mapping
  movq $0, paging_temp_tables

  // remove PDPT lower half mapping
  movq $0, (paging_temp_tables + (512*8))

  ret
