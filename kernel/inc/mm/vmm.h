#pragma once
#include "types.h"

// virtual memory areas
#define VMM_VMA_KERNEL     (0xffff800000000000)
#define VMM_VMA_KERNEL_END (0xffffffffffffffff)
#define VMM_VMA_USER       (0x0000000000000000)
#define VMM_VMA_USER_END   (0x00007fffffffffff)

// paging stuff
#define VMM_PAGE_SIZE         (4096)
#define VMM_PAGING_LEVEL      (4)
#define VMM_TABLE_ENTRY_COUNT (512)
#define VMM_TABLE_ENTRY_SIZE  (8)

// paging entry flags (https://wiki.osdev.org/Paging#Page_Directory)
#define VMM_FLAG_P   (1)         // present
#define VMM_FLAG_RW  (1 << 1)    // read/write
#define VMM_FLAG_US  (1 << 2)    // user/supervisor
#define VMM_FLAG_PWT (1 << 3)    // page write through
#define VMM_FLAG_PCD (1 << 4)    // page cache disable
#define VMM_FLAG_A   (1 << 5)    // accessed
#define VMM_FLAG_D   (1 << 6)    // dirty
#define VMM_FLAG_PAT (1 << 7)    // page attribute table
#define VMM_FLAG_PS  (1 << 7)    // page size
#define VMM_FLAG_G   (1 << 8)    // global
#define VMM_FLAG_XD  (1UL << 63) // execute disable (64 bit only, https://wiki.osdev.org/File:64-bit_page_tables2.png)

// some preset page flags combinations
#define VMM_FLAGS_DEFAULT (VMM_FLAG_P | VMM_FLAG_RW)
#define VMM_FLAGS_CLEAR                                                                                                \
  (~(VMM_FLAG_P | VMM_FLAG_RW | VMM_FLAG_US | VMM_FLAG_PWT | VMM_FLAG_PCD | VMM_FLAG_A | VMM_FLAG_D | VMM_FLAG_PAT |   \
      VMM_FLAG_G | VMM_FLAG_XD))

#ifndef __ASSEMBLY__

typedef uint64_t vmm_reg_t[2]; // defines a memory region, length and the starting address
#define vmm_reg_addr(r) (NULL == r ? 0 : r[1])
#define vmm_reg_len(r)  (NULL == r ? 0 : r[0])
#define vmm_reg_end(r)  (NULL == r ? 0 : r[1] + r[0])
#define vmm_reg_set_addr(r, a)                                                                                         \
  if (NULL != r) {                                                                                                     \
    r[1] = a;                                                                                                          \
  }
#define vmm_reg_set_len(r, l)                                                                                          \
  if (NULL != r) {                                                                                                     \
    r[0] = l;                                                                                                          \
  }
#define vmm_reg_init(r)                                                                                                \
  if (NULL != r) {                                                                                                     \
    r[0] = 0;                                                                                                          \
    r[1] = 0;                                                                                                          \
  }

// memory region functions
bool vmm_reg_clear(vmm_reg_t r); // clears a memory region
bool vmm_reg_check(vmm_reg_t r); // checks a memory region

// initializes the virtual memory manager with default max/min addresses
bool vmm_init();

// allocation functions
void *vmm_alloc(uint64_t size);
void *vmm_realloc(void *mem, uint64_t size);
void  vmm_free(void *mem);

void   *vmm_new();                            // create a new VMM
int32_t vmm_switch(void *vmm);                // switch to a different VMM
void    vmm_unmap(void *vaddr, uint64_t num); // unmap num amount of pages from the given virtual address

/*

 * map num amount of contigous pages with given page flags
 * VMA of the pages are auto detected (user VMA if US is set)

*/
void *vmm_map(uint64_t num, uint64_t flags);

/*

 * map num amount of contigous pages, physically aligned to given boundry
 * in the given VMA with the given flags

*/
void *vmm_map_with(uint64_t num, uint64_t align, uint64_t vma, uint64_t flags);

/*

 * map num amount of pages to the given virtual address, physically aligned to given
 * boundry, in the given VMA with given flags

*/
void *vmm_map_at(uint64_t vaddr, uint64_t num, uint64_t align, uint64_t vma, uint64_t flags);

#endif
