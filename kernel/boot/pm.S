.intel_syntax noprefix

#define ASM_FILE 1
#include "mm/pm.h"

/*

 * paging format is somehow more fucking painful then GDT
 * i swear to fucking god whoever designed this shit needs be tortured for eternity
 * like put his ass in fnaf 6 or something this mf is more evil then afton

 * anyway so im not gonna build another fucking table just check out the pictures from:
 * https://wiki.osdev.org/File:64-bit_page_tables1.png

 * also see check this out:
 * https://github.com/dreamportdev/Osdev-Notes/blob/master/04_Memory_Management/03_Paging.md#pml4--pdpr--pd

 * but basically each table needs to point to the other one, and an entry in a table
 * does not actually contain the full address, first 12 bits is removed because tables
 * are always aligned with 4096 bytes (min) first 12 bits are always zero

 * a part from that we don't really need to worry about other sections of the entry,
 * we'll just present and read/write bit for each of the entries, which is the first 2 bits,
 * see PM_ENTRY_FLAGS_DEFAULT

 * also i would like to thank zid` from the IRC for explaining the address resolve process
 * the explanation of the address resolve process from the first link uses 3 level paging
 * which i didn't realize and it confused me because i'm a massive fucking idiot

 * here's an image for 4 level paging:
 * https://de-academic.com/pictures/dewiki/88/X86_pm_64bit.svg

 * also god damn it heat_ was right i just should've used larger pages but fuck it
 * it's too late now, im to deep in this recursive assembly functions and shit, i can't go back now

*/

.section .data
  // stores the last mapped memory address
  pm_mapped: .skip 8
  pm_end:    .int 0

.section .text
.code32

.type __pm_get,   @function
.type __pm_setup, @function

.global __pm_get
.global __pm_setup

.global pm_mapped
.global pm_end

/*

 * so this does lots of stuff
 * - finds available memory regions and the memory limit using multiboot mmap info (__mb_get_region)
 * - uses the available memory region to identity map the memory up to the found limit
 * - returns the table address for PML4, which the loader.s loads to cr3

*/
__pm_get:
  push ebp
  mov ebp, esp

  sub esp, 2*4 // a list for the different table offsets, so [PML4, PDPT, PDT, PT]
  sub esp, 8   // used to store a copy of "mb_mem_avail_addr" and "mb_mem_avail_len"

  // save used registers
  push ebx
  push ecx
  push edx
  push esi
  push edi

  // get the first param (multiboot address)
  // and pass it to multiboot function
  mov ebx, [ebp+8]
  push ebx
  call __mb_get_mem

  /*

   * we are gonna obtain all the table indexes (offsets) for each level
   * of paging tables from the limit (maximum memory address available)

   * so currently the stack status is:
   * - [ebp - 2] = PT offset
   * - [ebp - 4] = PDT offset
   * - [ebp - 6] = PDPT offset
   * - [ebp - 8] = PML4 offset
   * - [ebp - 12] = mb_mem_avail_addr copy
   * - [ebp - 16] = mb_mem_avail_len copy

  */
  xor eax, eax
  xor ebx, ebx

  /*

   * first 12 bits is for the memory page offset
   * next 9 bits is for the PT offset
   * we can obtain this by shifting 12 bits right
   * and masking everything but the last 9 bits

  */
  mov ebx, [mb_mem_avail_limit+0]
  shr ebx, 12
  and ebx, 0x1FF
  // now we only have the 9 bits (PT offset)
  mov word ptr [ebp-2], bx

  /*

   * next 9 bits contain the PDT offset
   * again the same thing, however this time we will shift 21 bits right
   * this way will skip over the memory page offset and the PT offset

  */
  mov ebx, [mb_mem_avail_limit+0]
  shr ebx, 21
  and ebx, 0x1FF
  // now we only have the next 9 bits in bx (PDT offset)
  mov word ptr [ebp-4], bx

  /*

   * for the PDPT offset, again, we will need the next 9 bits
   * this time it's more complicated however,
   * as the first 2 bits of the 9 bits are located in the lower half
   * and the next 7 bits of the 9 bits are located in the higher half
   * i'll shift right the first 7 bits from the higher half
   * then ill shift 23 more to get all the 9 bits
   * and yes, lastly ill mask the rest of the bits

  */
  mov ebx, [mb_mem_avail_limit+0]

  // for the PDPT offset we will need 7 bits from the higher address
  mov eax, [mb_mem_avail_limit+4]
  shrd ebx, eax, 7

  shr ebx, 23
  and ebx, 0x1FF
  // now we only have the next 9 bits in bx (PDPT offset)
  mov word ptr [ebp-6], bx

  /*

   * for the PML4 offset, all the 9 bits are located in the higher half,
   * so we'll do the exact same thing we did to obtain PT and PDT offset
   * but this time we'll use the higher half

  */
  mov ebx, [mb_mem_avail_limit+4]
  shr ebx, 7
  and ebx, 0x1FF
  // now we only have the next 9 bits in bx (PML4 offset)
  mov word ptr [ebp-8], bx

  mov eax, [mb_mem_avail_addr+0] // eax has the mb_mem_avail_addr value
  mov ebx, [mb_mem_avail_len+0]  // ebx has the mb_mem_avail_len value

  mov [ebp-12], eax // [ebp - 12] now has a copy of mb_mem_avail_addr
  mov [ebp-16], ebx // [ebp - 16] now has a copy of mb_mem_avail_len

  // setup the params for __pm_setup_tables
  push ebp  // size pointer
  sub dword ptr [esp+0], 16
  push true // last entry?
  push 0    // level
  push ebp  // address pointer
  sub dword ptr [esp+0], 12
  push ebp  // table offset list
  sub dword ptr [esp+0], 8

  // setup the tables
  call __pm_setup_tables

  // remove the parameters on the stack
  add esp, 20

  /*

   * we'll save the paging end address, which should be located at where we copied the
   * "mb_mem_avail_addr", as this is modified by the setup function

  */
  mov ebx, [ebp-12]
  mov [pm_end+0], ebx

  // restore registers
  pop edi
  pop esi
  pop edx
  pop ecx
  pop ebx

  mov esp, ebp
  pop ebp

  ret

/*

 * sets up the paging tables according to given offsets
 * tables will be stored in the given memory region
 * and the proccess will stop if the region fills up

*/
__pm_setup_tables:
  push ebp
  mov ebp, esp

  // for local variables
  sub esp, 20

  // save used registers
  push ebx
  push ecx
  push edx
  push esi
  push edi

  mov eax, [ebp+8]  // eax will store the table offset list, so a uint16_t list of [PML4, PDPT, PDT, PT]
  mov ebx, [ebp+12] // ebx will store the pointer of address for the memory region which is used to store the tables
  mov ecx, [ebp+16] // ecx will store the current paging table level
  mov edx, [ebp+20] // edx will be set to true if this is the last table
  mov esi, [ebp+24] // esi will store the pointer of current size for the memory region (size for ebx)

  mov dword ptr [ebp-4], eax
  mov dword ptr [ebp-8], ebx
  mov dword ptr [ebp-12], ecx
  mov dword ptr [ebp-16], edx
  mov dword ptr [ebp-20], esi

  /*

   * current stack status:
   * [ebp - 4]  = table offset list
   * [ebp - 8]  = address pointer
   * [ebp - 12] = table level
   * [ebp - 16] = last table?
   * [ebp - 20] = size pointer

  */

  mov eax, [ebp-8]  // eax now stores the address pointer
  mov ebx, [ebp-20] // ebx now stores the size pointer
  mov edi, [eax+0]  // edi now has the current table address

  cmp dword ptr [ebx+0], PM_PAGE_SIZE // if there's no memory left, fail
  jb .Lpm_setup_tables_fail

  add dword ptr [eax+0], PM_PAGE_SIZE
  sub dword ptr [ebx+0], PM_PAGE_SIZE

  xor ecx, ecx // ecx used as a counter

  .Lpm_setup_tables_clear:
    // incerement the counter
    inc ecx

    // clear the current entry (see PM_ENTRY_SIZE)
    mov dword ptr [edi+0+((ecx-1)*PM_ENTRY_SIZE)], 0
    mov dword ptr [edi+4+((ecx-1)*PM_ENTRY_SIZE)], 0

    // if the counter reaches the max entry count
    // then we cleared the all the entries and we can exit the loop
    cmp ecx, PM_ENTRY_MAX
    jbe .Lpm_setup_tables_clear

  cmp dword ptr [ebp-12], PM_LEVEL-1
  jne .Lpm_setup_tables_enter_loop

  xor edx, edx

  mov eax, [ebp-12]              // eax now stores the current level
  mov ebx, [ebp-4]               // ebx now stores the offset table
  mov dx, word ptr [ebx+(eax*2)] // edx now stores the current level entry offset
  add edx, 1                     // edx now stores the current level entry count
  mov eax, PM_ENTRY_MAX      // eax now stores the maximum entry count
  cmp dword ptr [ebp-16], false  // is this the last table?
  cmove edx, eax                 // if not use the maximum entry count instead of the current level count

  mov eax, [pm_mapped+0] // eax now stores the mapped memory (lower)
  mov ebx, [pm_mapped+4] // ebx now stores the mapped memory (higher)
  xor ecx, ecx               // ecx used as a counter

  /*

   * eax = mapped memory address (lower)
   * ebx = mapped memory address (higher)
   * ecx = counter
   * edx = entry count (counter limit)

  */
  .Lpm_setup_tables_map_all:
    // set the current entry
    mov dword ptr [edi+0+(ecx*PM_ENTRY_SIZE)], eax
    mov dword ptr [edi+4+(ecx*PM_ENTRY_SIZE)], ebx
  
    // set the flags for the entry
    or dword ptr [edi+0+(ecx*PM_ENTRY_SIZE)], PM_ENTRY_FLAGS_DEFAULT
      
    // increment the address for the next entry
    add eax, PM_PAGE_SIZE
    adc ebx, 0

    mov [pm_mapped+0], eax
    mov [pm_mapped+4], ebx
    
    // increment the counter
    inc ecx
      
    // check if we are at the end
    cmp ecx, edx
    jl .Lpm_setup_tables_map_all

  jmp .Lpm_setup_tables_end

  .Lpm_setup_tables_enter_loop:
    xor ecx, ecx
    mov eax, [ebp-12]              // eax now stores the current level
    mov ebx, [ebp-4]               // ebx now stores the offset table
    mov cx, word ptr [ebx+(eax*2)] // ecx now stores the current level entry offset
    mov ebx, ecx                   // ebx now also stores the current level entry offset
    add ecx, 1                     // ecx now stores the current level entry count
    mov edx, PM_ENTRY_MAX      // edx now stores the max entry count
    cmp dword ptr [ebp-16], false  // is this the last table?
    cmove ecx, edx                 // if not use the maximum entry count instead of the current level count

    xor edx, edx // edx is used as a counter

    /*

     * eax = true if this the last entry
     * ebx = level entry offset
     * ecx = entry count (if last, then PM_ENTRY_MAX else ebx+1)
     * edx = counter

    */
    .Lpm_setup_tables_loop:
      mov eax, false

      // if we reach the end, we will return
      cmp edx, ecx
      jae .Lpm_setup_tables_end

      // is this the last table?
      cmp dword ptr [ebp-16], true
      jne .Lpm_setup_tables_loop_skip_last

      // is this the last entry?
      cmp edx, ebx
      jne .Lpm_setup_tables_loop_skip_last

      // if so, then this entry should be set as last
      .Lpm_setup_tables_loop_set_last:
        mov eax, true
      
      .Lpm_setup_tables_loop_skip_last:
        // recursive call
        mov esi, [ebp-12] // esi now stores the current level
        inc esi           // esi now stores the next level

        push dword ptr [ebp-20] // size pointer
        push eax                // last entry?
        push esi                // level
        push [ebp-8]            // address pointer
        push [ebp-4]            // table offset list

        call __pm_setup_tables

      // remove calling params
      add esp, 20

      // get the returned address
      mov dword ptr [edi+0+(edx*PM_ENTRY_SIZE)], eax
      mov dword ptr [edi+4+(edx*PM_ENTRY_SIZE)], 0

      // set the table entry flags
      or dword ptr [edi+0+(edx*PM_ENTRY_SIZE)], PM_ENTRY_FLAGS_DEFAULT

      // continue
      inc edx
      jmp .Lpm_setup_tables_loop 

  .Lpm_setup_tables_fail:
    xor edi, edi

  .Lpm_setup_tables_end:
    mov eax, edi // eax now stores the table address

    // restore registers
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx

    mov esp, ebp
    pop ebp

    ret
