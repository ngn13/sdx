ENTRY(__entry)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {

  . = 0x0;

  .text ALIGN(4K) : {
    KEEP(*(.multiboot))
    *(.text.*)
    *(.rodata)
  }

  .data : {
    *(.data)
  }

  /DISCARD/ : {
    *(.comment)
  }

  .end : {
    *(.end)
  }
}
