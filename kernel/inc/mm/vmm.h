#pragma once
#include "boot/boot.h"
#include "util/math.h"
#include "types.h"

// paging stuff
#define VMM_PAGE_SIZE         (4096)
#define VMM_PAGING_LEVEL      (4)
#define VMM_TABLE_ENTRY_COUNT (512)
#define VMM_TABLE_ENTRY_SIZE  (8)

// virtual memory areas
#define VMM_VMA_USER     (0x0000000000000000 + VMM_PAGE_SIZE) // 0x0 can be interpreted with NULL
#define VMM_VMA_USER_END (0x00007fffffffffff)

#define VMM_VMA_KERNEL     (0xffff800000000000)
#define VMM_VMA_KERNEL_END (BOOT_KERNEL_START_VADDR)

// paging entry flags (https://wiki.osdev.org/Paging#Page_Directory)
#define VMM_FLAG_P        (1)         // present
#define VMM_FLAG_RW       (1 << 1)    // read/write
#define VMM_FLAG_US       (1 << 2)    // user/supervisor
#define VMM_FLAG_PWT      (1 << 3)    // page write through
#define VMM_FLAG_PCD      (1 << 4)    // page cache disable
#define VMM_FLAG_A        (1 << 5)    // accessed
#define VMM_FLAG_D        (1 << 6)    // dirty
#define VMM_FLAG_PAT      (1 << 7)    // page attribute table
#define VMM_FLAG_PS       (1 << 7)    // page size
#define VMM_FLAG_G        (1 << 8)    // global
#define VMM_FLAG_XD       (1UL << 63) // execute disable (64 bit only, https://wiki.osdev.org/File:64-bit_page_tables2.png)
#define VMM_FLAGS_DEFAULT (VMM_FLAG_P | VMM_FLAG_RW) // default flags

#ifndef __ASSEMBLY__

int32_t vmm_init(); // setup all the required stuff for the VMM

void   *vmm_new();             // create a new VMM
void   *vmm_get();             // get the current VMM
int32_t vmm_sync(void *vmm);   // sync an old VMM with the current one
int32_t vmm_switch(void *vmm); // switch to a different VMM

#define vmm_save() void *_saved_vmm = vmm_get() // save the current VMM
#define vmm_restore()                                                                                                  \
  do {                                                                                                                 \
    if (vmm_get() == _saved_vmm)                                                                                       \
      break;                                                                                                           \
    vmm_sync(_saved_vmm);                                                                                              \
    vmm_switch(_saved_vmm);                                                                                            \
  } while (0) // sync the saved VMM with the current one and switch back to it

uint64_t vmm_resolve(void *vaddr); // resolve a virtual address to a physical address
uint64_t vmm_vma(void *vaddr);     // get the VMA that contains the given virtual address

int32_t vmm_set(void *vaddr, uint64_t num, uint64_t flags);   // set the specified flags for the vaddr entry
int32_t vmm_clear(void *vaddr, uint64_t num, uint64_t flags); // clear the specified flags for the vaddr entry
#define vmm_calc(size) (div_ceil(size, VMM_PAGE_SIZE))
int32_t vmm_unmap(void *vaddr, uint64_t num); // unmap num amount of pages from the given virtual address

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

 * map num amount of pages starting from the given physical address to an available
 * virtual address in the given VMA with the given flags

*/
void *vmm_map_to_paddr(uint64_t paddr, uint64_t num, uint64_t vma, uint64_t flags);

/*

 * map num amount of pages to the given virtual address, physically aligned to given
 * boundry, with the given flags

*/
void *vmm_map_to_vaddr(uint64_t vaddr, uint64_t num, uint64_t align, uint64_t flags);

#endif
