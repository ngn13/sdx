#include "boot/multiboot.h"
#include "boot/boot.h"

.global __mb_check
.global __mb_get_tag

.type __mb_check,   @function
.type __mb_get_tag, @function

.section .multiboot

/*

 * this section contains the multiboot header which is required by GRUB
 * this will be placed on top of the kernel ELF binary, see config/link.ld

 * to learn more about the multiboot2 specification see:
 * https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html

*/

multiboot_start:
  .int MULTIBOOT2_HEADER_MAGIC          // magic number
  .int MULTIBOOT_ARCHITECTURE_I386      // architecture (0 = i386)
  .int multiboot_end -  multiboot_start // multiboot header length

  // checksum
  .int -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_end - multiboot_start))

  // address tag
  .align 8
  multiboot_address_tag_start:
    .short MULTIBOOT_HEADER_TAG_ADDRESS
    .short MULTIBOOT_HEADER_TAG_OPTIONAL
    .int multiboot_address_tag_end - multiboot_address_tag_start

    .int multiboot_start - BOOT_KERNEL_ADDR // header_addr
    .int _start - BOOT_KERNEL_ADDR          // load_addr
    .int _edata - BOOT_KERNEL_ADDR          // load_end_addr
    .int _end - BOOT_KERNEL_ADDR            // bss_end_addr
  multiboot_address_tag_end:

  // entry address tag
  .align 8
  multiboot_entry_tag_start:
    .short MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
    .short MULTIBOOT_HEADER_TAG_OPTIONAL
    .int multiboot_entry_tag_end - multiboot_entry_tag_start

    .int __entry - BOOT_KERNEL_ADDR // entry_addr
  multiboot_entry_tag_end:

  .align 8
  .short MULTIBOOT_HEADER_TAG_END
  .short 0
multiboot_end:

.section .data
// error messages
err_no_magic: .string "HALTING: Magic number is not set (did you boot with GRUB multiboot2?)"
err_bad_len:  .string "HALTING: Multiboot information size is invalid"

.section .text
.code32

__mb_check:
  push %ecx

  cmp $MULTIBOOT2_BOOTLOADER_MAGIC, %eax
  jne .Lmb_check_fail

  mov (%ebx), %eax
  test %eax, %eax
  je .Lmb_len_fail

  pop %ecx
  ret

  .Lmb_check_fail:
    mov $(err_no_magic - BOOT_KERNEL_ADDR), %eax
    push %eax
    call __fail

  .Lmb_len_fail:
    mov $(err_bad_len - BOOT_KERNEL_ADDR), %eax
    push %eax
    call __fail

// returns a multiboot tag with a given type
__mb_get_tag:
  push %ebp
  mov %esp, %ebp

  // save used registers
  push %ebx
  push %ecx
  push %esi
  push %edi

  mov  8(%ebp), %ebx // ebx stores the multiboot data address (first arg)
  mov 12(%ebp), %ecx // ecx stores the requested tag type (second arg)

  /*

   * the tag list is offseted at 8 bytes
   * so with this addition ebx is pointing to the first tag

  */
  add $8, %ebx

  .Lmb_get_tag_find:
    /*

     * eax stores the return value (found tag address)
     * as we are in the loop, it will store the previous tag
     * and we can clear it everytime

    */
    xor %eax, %eax

    // a single tag contains a type and size (both 32 bits)
    mov  (%ebx), %esi // esi = tag type
    mov 4(%ebx), %edi // edi = tag size

    // did we reach the end? if so break
    cmp $MULTIBOOT_TAG_TYPE_END, %esi
    je .Lmb_get_tag_end

    /*

     * before the comparison, we'll move onto the next tag
     * this next tag's position can be calulcated with:
     * current address + size + 8 byte alignment padding

    */
    add  $7, %edi
    and $-8, %edi

    mov %ebx, %eax // save old tag address
    add %edi, %ebx // move onto next tag

    // if we find the tag we want, we can break out the loop
    cmp %esi, %ecx
    jne .Lmb_get_tag_find

  .Lmb_get_tag_end:
    pop %edi
    pop %esi
    pop %ecx
    pop %ebx

    mov %ebp, %esp
    pop %ebp

    ret
