OUTPUT_FORMAT(elf64-x86-64)
ENTRY(__entry)

KERNEL_VMA = 0xffffffff80000000;

SECTIONS {
  . = KERNEL_VMA;
  _start = .;

  .text : AT(ADDR(.text) - KERNEL_VMA) {
    KEEP(*(.multiboot))
    *(.text*)
  }

  .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
    *(.rodata*)
  }

  .data : AT(ADDR(.data) - KERNEL_VMA) {
    *(.data*)
  }

  _edata = .;

  .bss : AT(ADDR(.bss) - KERNEL_VMA) {
    *(.bss*)
    *(COMMON)
  }

  _end = .;

  /DISCARD/ : {
    *(.comment)
    *(note.*)
  }
}
