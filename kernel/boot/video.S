#include "boot/multiboot.h"

.equ VIDEO_FB_CHAR_SIZE, 2 // bytes per char
.equ VIDEO_FB_CHAR_SIZE_BITS, VIDEO_FB_CHAR_SIZE*8 // bytes per char

.type __video_init,  @function
.type __video_write, @function

.global __video_init
.global __video_write

.section .data

// error messages
err_video_bad_fb:   .string "HALTING: Framebuffer address is not accessible"
err_video_no_fb:    .string "HALTING: No framebuffer tag found in multiboot information"
err_video_bad_bpp:  .string "HALTING: Unsupported framebuffer charachter size"
err_video_bad_type: .string "HALTING: Unsupported framebuffer type"

// stores data extracted from framebuffer multiboot tag
video_fb_addr:   .int 0xb8000
video_fb_width:  .int 50
video_fb_height: .int 25

.section .text
.code32

/*

 * this function does few different things:
 * - checks framebuffer type and char size to make sure it's supported
 * - stores the framebuffer address in "video_fb_addr"
 * - stores the framebuffer height in "video_fb_height"
 * - stores the framebuffer width in "video_fb_width"
 * - if anything goes wrong, calls __fail, which halts the system

 * all of these are used for temporary video output with __video_write
 * we have a better framebuffer driver in c, which we can use when we switch to long mode

*/
__video_init:
  push %ebp
  mov %esp, %ebp

  // save used registers
  push %ebx
  push %ecx

  mov 8(%ebp), %eax // eax will store the multiboot data address (first arg)

  // obtain the tag for framebuffer info
  push $MULTIBOOT_TAG_TYPE_FRAMEBUFFER
  push %eax
  call __mb_get_tag

  // remove argument from the stack
  add $8, %esp

  // if we don't have a tag for framebuffer, then fail
  test %eax, %eax
  je .Lvideo_init_find_fail

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

  mov  8(%eax), %ebx // ebx = framebuffer_addr (lower)
  mov 12(%eax), %ecx // ecx = framebuffer_addr (higher)

  // if the address is larger than 32 bits, then we won't be able to access it
  test %ecx, %ecx
  jne .Lvideo_init_bad_addr

  mov %ecx, video_fb_addr

  mov 20(%eax), %ecx // ecx = framebuffer_width
  mov %ecx, video_fb_width

  mov 24(%eax), %ecx // ecx = framebuffer_height
  mov %ecx, video_fb_height

  /*

   * bpp is bits per byte, so bits per char, we need to make sure
   * it matches with bytes per char amount we are using

  */
  mov 28(%eax), %cl // cl = framebuffer_bpp
  cmp $VIDEO_FB_CHAR_SIZE_BITS, %cl
  jne .Lvideo_init_bad_bpp

  /*

   * check the framebuffer type, make sure its a supported one
   * currently all the types are supported, but it wouldn't hurt
   * to check it anyway

  */
  mov 29(%eax), %cl // cl = framebuffer_type
  cmp $MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT, %cl
  jne .Lvideo_init_bad_type

  // restore the registers and return
  pop %ecx
  pop %ebx

  mov %ebp, %esp
  pop %ebp

  ret

  .Lvideo_init_find_fail:
    push $err_video_no_fb
    call __fail

  .Lvideo_init_bad_addr:
    push $err_video_bad_fb
    call __fail

  .Lvideo_init_bad_bpp:
    push $err_video_bad_bpp
    call __fail

  .Lvideo_init_bad_type:
    push $err_video_bad_type
    call __fail

__video_write:
  push %ebp
  mov %esp, %ebp

  // save used registers
  push %eax
  push %ebx
  push %ecx
  push %esi

  mov  8(%ebp), %eax   // eax stores the message (first argument)
  mov 12(%ebp), %ebx   // ebx stores the color (second argument)
  xor %ecx, %ecx       // ecx stores the current char
  mov video_fb_addr, %esi // esi stores the framebuffer address

  mov %bl, %ch // ch = color

  .Lvideo_write_char:
    mov (%eax), %cl // cl = current char

    /*

     * if the current char is 0 (null terminator)
     * this means we reached the end, so at this point
     * we can break out of the loop

    */
    test %cl, %cl
    je .Lvideo_write_char_end

    mov %cx, (%esi)

    inc %eax
    add $VIDEO_FB_CHAR_SIZE, %esi
    jmp .Lvideo_write_char

  .Lvideo_write_char_end:
    // restore registers
    pop %esi
    pop %ecx
    pop %ebx
    pop %eax

    mov %ebp, %esp
    pop %ebp

    ret
