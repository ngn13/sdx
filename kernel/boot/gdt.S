.intel_syntax noprefix

.section .data
.align 16

/*
  each descriptor is 8 bytes, here's the format for a GDT descriptor

  |   7 |             6 |      5 | 4 - 2 | 1 - 0 |
  | --- | ------------- | ------ | ----- | ----- |
  | ptr | flags + limit | access |   ptr | limit |
  
  who the fuck came up with this shit dawg

  anyway we are gonna use the max limit (set all the bits) for the segments
  and we are gonna use 0 for the pointer (base address)

  to learn more about flag and access bits, see this page from osdev:
  https://wiki.osdev.org/Global_Descriptor_Table

  it explains everything very well with tables
*/

gdt_start:
  // first GDT descriptor should be empty (NULL descriptor) 
  gdt_desc_null:
    .quad 0

  // code segment
  gdt_desc_code:
    // limit
    .short 0xffff

    // pointer
    .short 0
    .byte 0
  
    // access
    // we are using (right to left)
    // RW, E, S, P
    .byte 0b10011010

    // flags + limit
    // for flags we are using (right to left)
    // G and L (64 bit flag)
    .byte 0b10101111

    // rest of the pointer
    .byte 0
   
  // data segment
  gdt_desc_data:
    .short 0xffff

    .short 0
    .byte 0

    // we are using (right to left)
    // RW, S, P
    .byte 0b10010010

    // for flags we are using (right to left)
    // G and DB (32 bit flag)
    .byte 0b11001111

    .byte 0
gdt_end:

gdt_pointer:
  .short gdt_end - gdt_start - 1
  .int gdt_start

gdt_desc_code_addr: .int gdt_desc_code
gdt_desc_data_addr: .int gdt_desc_data

gdt_start_addr: .int gdt_start
gdt_end_addr: .int gdt_end

.global gdt_desc_code_addr
.global gdt_desc_data_addr

.global gdt_start_addr
.global gdt_end_addr

.global gdt_pointer
