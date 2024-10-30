.intel_syntax noprefix

.section .text
.code32

.type __video_init,  @function
.type __video_write, @function

.global __video_init
.global __video_write

__video_init:
  push ebp
  mov ebp, esp

  // save used registers
  push ebx

  // get the first param (multiboot address)
  // and pass it to multiboot function
  mov ebx, [ebp+8]
  push ebx
  call __mb_get_fb

  /*

   * normally i would do a framebuffer type check here,
   * using "mb_fb_type", however all the framebuffer types
   * are actually supported by these video functions

   * so (at least for now) we can skip that

  */

  // restore the registers
  pop ebx

  mov esp, ebp
  pop ebp
  
  ret

__video_write:
  push ebp
  mov ebp, esp

  // save used registers
  push eax
  push ebx
  push ecx
  push esi

  mov eax, [ebp+8]        // eax stores the message (first argument)
  mov ebx, [ebp+12]       // ebx stores the color (second argument)
  xor ecx, ecx            // ecx stores the current char
  mov esi, [mb_fb_addr+0] // esi stores the mb_fb_addr

  .Lvideo_write_char:
    mov cl, [eax+0] // cl = current char
    mov ch, bl      // ch = color
   
    // if the current char is 0 (null terminator)
    // then break out of the loop
    test cl, cl
    je .Lvideo_write_char_end

    mov word ptr [esi], cx

    inc eax
    add esi, [mb_fb_size+0]
    jmp .Lvideo_write_char
    
  .Lvideo_write_char_end:
    pop esi
    pop ecx
    pop ebx
    pop eax
 
    mov esp, ebp
    pop ebp
  
    ret 
