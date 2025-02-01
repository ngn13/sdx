.global gdt_pointer
.global gdt_start_addr
.global gdt_end_addr
.global gdt_desc_code_0_addr
.global gdt_desc_data_0_addr
.global gdt_desc_code_3_addr
.global gdt_desc_data_3_addr
.global gdt_desc_tss_addr

.type gdt_pointer,          @function
.type gdt_start_addr,       @function
.type gdt_end_addr,         @function
.type gdt_desc_code_0_addr, @function
.type gdt_desc_data_0_addr, @function
.type gdt_desc_code_3_addr, @function
.type gdt_desc_data_3_addr, @function
.type gdt_desc_tss_addr,    @function

.section .data
.align 16

/*

 * in antique times, when dinosaurs used to jerk each other off,
 * there existed 16 bit microprocessors, now these bad boys could
 * work with 64KB of memory which was a lot back then

 * but as the time went on, all those engineers realized if they
 * keep working with 64KB of memory, they will never be able to run fortnite
 * and they sure won't be able hit the griddy

 * but adding more bits required bunch of work that they didn't want to do
 * so they decided lets add some more registers so we can use them to offset into
 * larger addresses, they called them the segment selector registers

 * but what they didn't realize is that this quest to address more memory to run
 * fornite, would end up creating the most diabolical data structure the world has
 * ever seen to this day

 * so lets break it down, these 16 bit segment registers have the following format:
 * - bit 0-1 : RPL (requested privilege level)
 * - bit 2   : TI
 * - bit 3-15: index

 * the most important field is the index, you see segment registers index into
 * something called the "descriptor table", each entry in this table defines a memory
 * segment: it the CPU where the segment is stored, where it ends, which permissions it has etc.

 * the TI field tells which descriptor table to use, there is a local descriptor table (LDT) and
 * a global descriptor table (GDT)

 * lastly the last field, RPL, tells the requested privilege levael (ring) we are gonna use,
 * if the segments privilege level is larger than the segment selectors request privilege level,
 * then CPU won't allow that operation

 * to understand how addresses are resolved with segment selectors see this video i found
 * on youtube while trying figure out this antique technology: https://www.youtube.com/watch?v=iqj3r0pKZA8&t=397

 * obv in 64 bit, segmentation is no longer needed, it's only here for legacy reasons,
 * however will still need it in long mode so we can tell the CPU which ring we are working on

 * to do so we are gonna setup a GDT, with entries for the kernel (ring 0) and ring (ring 3) code/data field
 * the actual parts of the GDT descriptors, where it stores the information about the segments address
 * and stuff are gonna be zero, cause we are not doing actual segmentation we are just using GDTs for setting
 * the ring AKA the privilege level

 * so lets breakdown diarrheal data structure, each descriptor in the descriptor table
 * is 8 bytes, here's the format:

 |   7 |             6 |      5 | 4 - 2 | 1 - 0 |
 | --- | ------------- | ------ | ----- | ----- |
 | ptr | flags + limit | access |   ptr | limit |

 * i know, go grab some bleach for your eyes

 * anyway we are gonna use the max limit (set all the bits) for the segments
 * and we are gonna use 0 for the pointer (base address), bc as i mentioned earlier we are not doing
 * actual segmentation

 * to learn more about flag and access bits, see this page from osdev:
 * https://wiki.osdev.org/Global_Descriptor_Table
 * also theres a system segment descriptor with a different format

 * it explains everything very well with tables

*/

gdt_start:
  // first GDT descriptor should be empty (NULL descriptor)
  gdt_desc_null:
    .quad 0

  // code segment (ring 0)
  gdt_desc_code_0:
    .short 0xffff // limit
    .short 0 // pointer
    .byte 0 // pointer
    .byte 0b10011010 // access (RW, E, S, P)
    .byte 0b10101111 // flags (G, L) and limit
    .byte 0 // pointer

  // data segment (ring 0)
  gdt_desc_data_0:
    .short 0xffff // limit
    .short 0 // pointer
    .byte 0 // pointer
    .byte 0b10010010 // access (RW, S, P)
    .byte 0b11001111 // flags (G, DB) and limit
    .byte 0 // pointer

  /*

   * you might be wondering why the ring 0 code segment comes first
   * while the ring 3 code segment doesn't, well you should see core/user.c to see
   * how intel decided to ass fuck my GDT with their shit MSR, yeah, fuck you intel

  */

  // data segment (ring 3)
  gdt_desc_data_3:
    .short 0xffff // limit
    .short 0 // pointer
    .byte 0 // pointer
    .byte 0b11110010 // access (DPL = 3, RW, S, P)
    .byte 0b11001111 // flags (G, DB) and limit
    .byte 0 // pointer

  // code segment (ring 3)
  gdt_desc_code_3:
    .short 0xffff // limit
    .short 0 // pointer
    .byte 0 // pointer
    .byte 0b11111010 // access (DPL = 3, RW, E, S, P)
    .byte 0b10101111 // flags (G, L) and limit
    .byte 0 // pointer

  /*

   * TSS segment, uses a different descriptor format
   * see https://wiki.osdev.org/Global_Descriptor_Table#Long_Mode_System_Segment_Descriptor

  */
  gdt_desc_tss:
    .short 0 // limit
    .short 0 // pointer
    .byte 0 // pointer
    .byte 0b11101001 // access (DPL = 3, A, E, P)
    .byte 0 // flags, limit
    .byte 0 // pointer
    .int 0 // pointer
    .int 0 // reserved

gdt_end:

gdt_pointer:
  .short gdt_end - gdt_start - 1
  .int gdt_start

gdt_start_addr:       .int gdt_start
gdt_end_addr:         .int gdt_end
gdt_desc_code_0_addr: .int gdt_desc_code_0
gdt_desc_data_0_addr: .int gdt_desc_data_0
gdt_desc_code_3_addr: .int gdt_desc_code_3
gdt_desc_data_3_addr: .int gdt_desc_data_3
gdt_desc_tss_addr:    .int gdt_desc_tss
