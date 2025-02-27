#pragma once

#define PAGE_SIZE    (4096)
#define PAGING_LEVEL (4)

#define PTE_COUNT (512)
#define PTE_SIZE  (8)

// paging entry flags (https://wiki.osdev.org/Paging#Page_Directory)
#define PTE_FLAG_P   (1)         // present
#define PTE_FLAG_RW  (1 << 1)    // read/write
#define PTE_FLAG_US  (1 << 2)    // user/supervisor
#define PTE_FLAG_PWT (1 << 3)    // page write through
#define PTE_FLAG_PCD (1 << 4)    // page cache disable
#define PTE_FLAG_A   (1 << 5)    // accessed
#define PTE_FLAG_D   (1 << 6)    // dirty
#define PTE_FLAG_PAT (1 << 7)    // page attribute table
#define PTE_FLAG_PS  (1 << 7)    // page size
#define PTE_FLAG_G   (1 << 8)    // global
#define PTE_FLAG_XD  (1UL << 63) // execute disable (64 bit only, https://wiki.osdev.org/File:64-bit_page_tables2.png)

/*

 * not an actual entry flag, bit 9 of the entry is free for use
 * so i use it to keep track of pages that are allocated from the PMM

*/
#define PTE_FLAG_PMM (1 << 9)

// flag combinations
#define PTE_FLAGS_DEFAULT (PTE_FLAG_P | PTE_FLAG_RW) // default flags
#define PTE_FLAGS_ALL                                                                                                  \
  (PTE_FLAG_P | PTE_FLAG_RW | PTE_FLAG_US | PTE_FLAG_PWT | PTE_FLAG_PCD | PTE_FLAG_A | PTE_FLAG_D | PTE_FLAG_PAT |     \
      PTE_FLAG_G | PTE_FLAG_XD) // all flags
#define PTE_FLAGS_CLEAR                                                                                                \
  (~(PTE_FLAG_P | PTE_FLAG_RW | PTE_FLAG_US | PTE_FLAG_PWT | PTE_FLAG_PCD | PTE_FLAG_A | PTE_FLAG_D | PTE_FLAG_PAT |   \
      PTE_FLAG_G | PTE_FLAG_XD | PTE_FLAG_PMM))
