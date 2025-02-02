#include "boot/multiboot.h"

.global __mb_check
.global __mb_get_tag
.global __mb_get_fb
.global mb_len
.global mb_fb_addr
.global mb_fb_width
.global mb_fb_height
.global mb_fb_type
.global mb_fb_size

.type __mb_check,   @function
.type __mb_get_tag, @function
.type __mb_get_fb,  @function
.type mb_len,       @common
.type mb_fb_addr    @common
.type mb_fb_width,  @common
.type mb_fb_height, @common
.type mb_fb_type,   @common
.type mb_fb_size,   @common

.section .multiboot

/*

 * this section contains the multiboot header which is required by GRUB
 * this will be placed on top of the kernel ELF binary, see config/link.ld
 * to learn more about the multiboot2 specification see:
 * https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html

*/
multiboot_start:
  .int 0xe85250d6                       // magic number
  .int 0                                // architecture (0 = i386)
  .int multiboot_end -  multiboot_start // multiboot header length

  // checksum
  .int 0x100000000 - (0xe85250d6 + 0 + (multiboot_end - multiboot_start))

  // the header tag
  .long 0 // type
  .long 0 // flags
  .long 8 // size
multiboot_end:

.section .data

// error messages
err_bad_fb_mb: .string "HALTING: Framebuffer address is not accessible"
err_no_fb_mb:  .string "HALTING: No framebuffer tag found in multiboot information"
err_no_magic:  .string "HALTING: Magic number is not set (did you boot with GRUB multiboot2?)"

// stores data extracted from the boot info
mb_len:       .int 0
mb_fb_addr:   .int  0xb8000
mb_fb_width:  .int 0
mb_fb_height: .int 0
mb_fb_type:   .byte MULTIBOOT_FRAMEBUFFER_TYPE_RGB
mb_fb_size:   .byte 2

.section .text
.code32

__mb_check:
  push %ecx

  cmp $MULTIBOOT2_BOOTLOADER_MAGIC, %eax
  jne .Lmb_check_fail

  mov (%ebx), %eax
  mov $mb_len, %ecx
  mov %eax, (%ecx)

  pop %ecx
  ret

  .Lmb_check_fail:
    mov $err_no_magic, %eax
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

/*

 * this function does few different things:
 * - stores the framebuffer address in "mb_fb_addr"
 * - stores the framebuffer char width in "mb_fb_width"
 * - stores the framebuffer type in "mb_fb_type"
 * - returns the address of "mb_fb_addr"
 * - if anything goes wrong, calls __fail, which halts the system

 * all of these are used for temporary video output, and they are
 * also used by the video driver after entering the long mode

*/
__mb_get_fb:
  push %ebp
  mov %esp, %ebp

  // save the used registers
  push %ebx
  push %ecx
  push %edx

  mov 8(%ebp), %eax // eax will store the multiboot data address (first arg)

  // obtain the tag for framebuffer info
  push $MULTIBOOT_TAG_TYPE_FRAMEBUFFER
  push %eax
  call __mb_get_tag

  // if we don't have a tag for framebuffer, then fail
  test %eax, %eax
  je .Lmb_get_fb_find_fail

  /*

   * if we got here, then eax stores the framebuffer multiboot tag
   * address, this tag has the following structure:

   struct multiboot_tag_framebuffer_common {
      uint32_t type;
      uint32_t size;

      uint64_t framebuffer_addr;
      uint32_t framebuffer_pitch;
      uint32_t framebuffer_width;
      uint32_t framebuffer_height;
      uint8_t  framebuffer_bpp;
      uint8_t  framebuffer_type;
      uint16_t reserved;
    };

    * we are interested in the following:
    * - framebuffer_addr
    * - framebuffer_bpp
    * - framebuffer_type

  */

  mov  8(%eax), %ecx // esi = framebuffer_addr (lower)
  mov 12(%eax), %edx // edi = framebuffer_addr (higher)

  // if the address is larger then 32 bits, then we won't be able to access it
  test %edx, %edx
  jne .Lmb_get_fb_bad_addr

  mov %ecx, mb_fb_addr

  mov 20(%eax), %ecx // ecx = framebuffer_width
  mov %ecx, mb_fb_width

  mov 24(%eax), %ecx // ecx = framebuffer_height
  mov %ecx, mb_fb_height

  mov 28(%eax), %cl // cl = framebuffer_bpp
  mov %cl, mb_fb_size

  mov 29(%eax), %dl // dl = framebuffer_type
  mov %dl, mb_fb_type

  // divide framebuffer_bpp with 8 (convert it to bytes)
  xor %eax, %eax
  mov %cl, %al
  mov $8, %cl
  div %cl
  mov %al, mb_fb_size

  // return the address of mb_fb_addr
  mov $mb_fb_addr, %eax

  // restore the registers and return
  pop %edx
  pop %ecx
  pop %ebx

  mov %ebp, %esp
  pop %ebp

  ret

  .Lmb_get_fb_find_fail:
    push $err_no_fb_mb
    call __fail

  .Lmb_get_fb_bad_addr:
    push $err_bad_fb_mb
    call __fail
