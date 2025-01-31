.type __video_init,  @function
.type __video_write, @function

.global __video_init
.global __video_write

.section .text
.code32

__video_init:
  // save used registers
  push %ebx

  /*

   * get the first param (multiboot address)
   * and pass it to multiboot function

  */
  mov 8(%esp), %ebx
  push %ebx
  call __mb_get_fb

  // remove argument from the stack
  add $4, %esp

  /*

   * normally i would do a framebuffer type check here,
   * using "mb_fb_type", however all the framebuffer types
   * are actually supported by these video functions

   * so (at least for now) we can skip that

  */

  // restore the registers
  pop %ebx
  ret

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
  mov mb_fb_addr, %esi // esi stores the framebuffer address

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
    add mb_fb_size, %esi
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
