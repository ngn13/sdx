.intel_syntax noprefix

// TODO: rewrite in GAS (AT&T) syntax

#define ASM_FILE 1
#include "boot/multiboot.h"

.section .multiboot

/*

 * multiboot header which is required by GRUB
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
  err_bad_mb_addr: .string "HALTING: Multiboot information address is invalid"
  err_no_mmap_mb:  .string "HALTING: No mmap tag found in multiboot information"
  err_bad_fb_mb:   .string "HALTING: Framebuffer address is not accessible"
  err_no_fb_mb:    .string "HALTING: No framebuffer tag found in multiboot information"

  // memory region vars
  mb_mem_avail_addr:  .int 0  // available 32 memory starting address
  mb_mem_avail_len:   .int 0  // available 32 memory length
  mb_mem_avail_limit: .skip 8 // available memory limit

  // framebuffer vars (with default values)
  mb_fb_addr:   .int  0xb8000
  mb_fb_width:  .int 0
  mb_fb_height: .int 0
  mb_fb_type:   .byte MULTIBOOT_FRAMEBUFFER_TYPE_RGB
  mb_fb_size:   .byte 2

.global mb_mem_avail_addr
.global mb_mem_avail_len
.global mb_mem_avail_limit
.global mb_mem_limit

.global mb_fb_addr
.global mb_fb_width
.global mb_fb_height
.global mb_fb_type
.global mb_fb_size

.section .text
.code32

.type __mb_tag,     @function
.type __mb_get_mem, @function
.type __mb_get_fb,  @function

.global __mb_get_mem
.global __mb_get_fb

// returns a multiboot tag with a given type
__mb_tag:
  push ebp
  mov ebp, esp

  // save used registers
  push ebx
  push ecx
  push esi
  push edi

  mov ebx, [ebp+8]  // ebx will store the multiboot data address (first param)
  mov ecx, [ebp+12] // ecx will store the tag type (second param)

  test ebx, ebx
  je .Lmb_tag_bad_addr

  // the tag list starts after 8 bytes
  add ebx, 8

  .Lmb_tag_find:
    // eax stores the return value (found tag address)
    // as we are in the loop, it will store the previous tag
    // and we can clear it everytime
    xor eax, eax

    // a single tag contains a type and size (both 32 bits)
    mov esi, [ebx+0] // esi = tag type
    mov edi, [ebx+4] // edi = tag size

    cmp esi, MULTIBOOT_TAG_TYPE_END
    je .Lmb_tag_end

    // next tag is located at:
    // current address + size + 8 byte alignment padding
    add edi, 7
    and edi, -8
    mov eax, ebx // save old tag address
    add ebx, edi // move onto next tag

    // if we find the tag we want, we can break out the loop
    cmp esi, ecx
    jne .Lmb_tag_find

  .Lmb_tag_end:
    pop edi
    pop esi
    pop ecx
    pop ebx

    mov esp, ebp
    pop ebp

    ret

  .Lmb_tag_bad_addr:
    lea eax, err_bad_mb_addr

  push eax
  call __fail

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
  push ebp
  mov ebp, esp

  // save the used registers
  push ebx
  push ecx
  push edx

  mov eax, [ebp+8] // eax will store the multiboot data address (first param)
  mov ebx, MULTIBOOT_TAG_TYPE_FRAMEBUFFER // ebx stores the tag type

  // obtain the tag for framebuffer info
  push ebx
  push eax
  call __mb_tag

  // if we don't have a tag for framebuffer, then fail
  test eax, eax
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

  mov ecx, [eax+8]  // esi = framebuffer_addr (lower)
  mov edx, [eax+12] // edi = framebuffer_addr (higher)

  // if the address is larger then 32 bits, then we won't
  // be able to access it
  test edx, edx
  jne .Lmb_get_fb_bad_addr

  mov [mb_fb_addr+0], ecx
  
  mov ecx, [eax+20] // ecx = framebuffer_width
  mov [mb_fb_width+0], ecx

  mov ecx, [eax+24] // ecx = framebuffer_height
  mov [mb_fb_height+0], ecx
  
  mov cl, [eax+28] // cl = framebuffer_bpp
  mov [mb_fb_size+0], cl

  mov dl, [eax+29] // dl = framebuffer_type
  mov [mb_fb_type+0], dl

  // divide framebuffer_bpp with 8 (convert it to bytes)
  xor eax, eax
  mov al, cl
  mov cl, 8
  div cl
  mov [mb_fb_size+0], al

  // return the address of mb_fb_addr
  lea eax, mb_fb_addr

  // restore the registers and return
  pop edx
  pop ecx
  pop ebx

  mov esp, ebp
  pop ebp

  ret

  .Lmb_get_fb_find_fail:
    lea eax, err_no_fb_mb
    push eax
    call __fail

  .Lmb_get_fb_bad_addr:
    lea eax, err_bad_fb_mb
    push eax
    call __fail

/*

 * this function does few different things:
 * - stores the available 32 bit memory start address in "mb_mem_avail_addr"
 * - stores the available 32 bit memory address length in "mb_mem_avail_len"
 * - stores the maximum 64 bit memory limit in "mb_mem_limit"
 * - returns the address of "mb_mem_limit"
 * - if anything goes wrong, calls __fail, which halts the system

 * these information is used to setup the paging, they also used by VMM
 * after entering the long mode

*/
__mb_get_mem:
  push ebp
  mov ebp, esp

  /*

   * local variables

   * [ebp - 4]  = tag address
   * [ebp - 8]  = tag size
   * [ebp - 12] = temporary
   * [ebp - 16] = temporary

  */
  sub esp, 16

  // save the used registers
  push ebx
  push ecx
  push edx
  push esi
  push edi

  mov ebx, [ebp+8] // ebx will store the multiboot data address (first param)
  mov ecx, MULTIBOOT_TAG_TYPE_MMAP // ecx stores the tag type

  // obtain the tag for mmap
  push ecx
  push ebx
  call __mb_tag

  // remove the pushed arguments
  add esp, 8

  // if we don't have a tag for mmap, then fail
  test eax, eax
  je .Lmb_get_mem_find_fail

  /*

   * if we got here, then eax stores the mmap tag address
   * for reference, here's the tag structure from multiboot.h

   struct multiboot_tag_mmap {
      uint32_t                    type;
      uint32_t                    size;
      uint32_t                    entry_size;
      uint32_t                    entry_version;
      struct multiboot_mmap_entry entries[0];
    };

    * also we don't need to check the entry_version, all versions
    * are backwards compatible

  */

  mov ebx, [eax+4] // ebx = size
  mov ecx, [eax+8] // ecx = entry_size
  add eax, 16      // eax = entries[0]

  xor esi, esi // used for higher address
  xor edi, edi // used for lower address

  /*

   * after this point we are looping over all the entries
   * again, for reference, here's the entry structure from multiboot.h:

   struct multiboot_mmap_entry {
      uint64_t addr;
      uint64_t len;
      uint32_t type;
      uint32_t zero;
   };

   * we also decrease the size with the entry_size each time
   * so we know when we hit the end

   * also i keep saying "we" cause like thats how would i explain
   * this in turkish, but what i really mean is "I" because your lazy
   * ass is not helping me write this, your just reading my comment

  */

  // lets save the tag address and size for the second loop
  mov dword ptr [ebp-4], eax
  mov dword ptr [ebp-8], ebx

  // first lets clear out the current address...
  mov dword ptr [mb_mem_avail_addr+0], 0

  // and the length
  mov dword ptr [mb_mem_avail_len+0], 0

  // first loop is for finding the available memory
  .Lmb_get_mem_find_avail:
    // check if the entry contains available memory
    mov esi, [eax+16]
    cmp esi, MULTIBOOT_MEMORY_AVAILABLE
    jne .Lmb_get_mem_find_avail_next

    // store the address
    mov esi, [eax+0] // lower
    mov edi, [eax+4] // higher

    // is it larger then 32 bits?
    test edi, edi
    jne .Lmb_get_mem_find_avail_next

    // if not lets get the length
    mov edi, 0xffffffff
    sub edi, esi

    // store the address and the length (temporary)
    mov dword ptr [ebp-12], esi // address
    mov dword ptr [ebp-16], edi // length

    // get the actual length
    mov esi, dword ptr [eax+8]  // lower
    mov edi, dword ptr [eax+12] // higher

    // is it smaller then the length we calculated?
    test edi, edi
    jne .Lmb_get_mem_find_avail_use_cur

    cmp esi, dword ptr [ebp-16]
    ja .Lmb_get_mem_find_avail_use_cur

    // if it's, then use the acutal length
    mov dword ptr [ebp-16], esi

    .Lmb_get_mem_find_avail_use_cur:
      /*

       * we now have the address and the length

       * just need to figure out if the length is larger then
       * the current "mb_mem_avail_len", then we'll use the new address instead

       * we also make sure that the address does not overwrite the kernel code
       * to do so we can check the address of .end section (see end.S and config/link.ld)

      */
      mov esi, dword ptr [ebp-12] // address
      mov edi, dword ptr [ebp-16] // length

      cmp esi, end_addr
      jl .Lmb_get_mem_find_avail_next

      cmp dword ptr [mb_mem_avail_len+0], edi
      jae .Lmb_get_mem_find_avail_next

      // if its larger then the current address length
      // then we found our new address
      mov dword ptr [mb_mem_avail_addr+0], esi
      mov dword ptr [mb_mem_avail_len+0], edi

    .Lmb_get_mem_find_avail_next:
      add eax, ecx // move onto next entry
      sub ebx, ecx // are we at the end yet
      ja .Lmb_get_mem_find_avail

  // reset tag address and size
  mov eax, dword ptr [ebp-4]
  mov ebx, dword ptr [ebp-8]

  // clear out the limit
  mov dword ptr [mb_mem_avail_limit+0], 0
  mov dword ptr [mb_mem_avail_limit+4], 0

  // second loop is to find the available memory limit
  .Lmb_get_mem_find_avail_limit:
    // check if the entry contains available memory
    mov esi, [eax+16]
    cmp esi, MULTIBOOT_MEMORY_AVAILABLE
    jne .Lmb_get_mem_find_avail_limit_next

    // store the address
    mov esi, [eax+0] // lower
    mov edi, [eax+4] // higher

    mov dword ptr [ebp-12], esi
    mov dword ptr [ebp-16], edi

    // get the length
    mov esi, [eax+8]
    mov edi, [eax+12]

    // get the end address
    add dword ptr [ebp-12], esi
    adc dword ptr [ebp-16], edi

    // if the address is larger than limit, set it as the new limit
    mov esi, dword ptr [ebp-12] // end address (lower)
    mov edi, dword ptr [ebp-16] // end address (higher)

    cmp dword ptr [mb_mem_avail_limit+4], edi
    jb .Lmb_get_mem_find_avail_limit_new
    ja .Lmb_get_mem_find_avail_limit_next

    cmp dword ptr [mb_mem_avail_limit+0], esi
    jb .Lmb_get_mem_find_avail_limit_new

    jmp .Lmb_get_mem_find_avail_limit_next

    .Lmb_get_mem_find_avail_limit_new:
      mov dword ptr [mb_mem_avail_limit+0], esi
      mov dword ptr [mb_mem_avail_limit+4], edi

    .Lmb_get_mem_find_avail_limit_next:
      add eax, ecx // move onto next entry
      sub ebx, ecx // are we at the end yet
      ja .Lmb_get_mem_find_avail_limit

  // return the available memory region address
  lea eax, mb_mem_avail_limit

  // restore registers and return
  pop edi
  pop esi
  pop edx
  pop ecx
  pop ebx

  mov esp, ebp
  pop ebp

  ret

  .Lmb_get_mem_find_fail:
    lea eax, err_no_mmap_mb

  push eax
  call __fail
