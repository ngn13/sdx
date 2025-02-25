#pragma once
#include "boot/boot.h"
#include "util/math.h"
#include "mm/pmm.h"

#include "errno.h"
#include "types.h"

// attributes for the VMM functions
#define VMM_ATTR_NO_EXEC  (1)      // pages should not be executable
#define VMM_ATTR_NO_CACHE (1 << 1) // pages should not be cached
#define VMM_ATTR_RDONLY   (1 << 2) // pages should be read-only
#define VMM_ATTR_USER     (1 << 3) // pages should be user pages (ring 3)
#define VMM_ATTR_SAVE     (1 << 4) // pages should not free hysical pages when unmapped
#define VMM_ATTR_REUSE    (1 << 5) // pages should reuse already mapped memory

// virutal memory areas
#define VMM_VMA_KERNEL (1)
#define VMM_VMA_USER   (2)

#ifndef __ASSEMBLY__

int32_t vmm_init();                                  // setup all the required stuff for the VMM
void   *vmm_new();                                   // create a new VMM
#define vmm_free(vmm) (pmm_free(((uint64_t)vmm), 1)) // free the given VMM
void   *vmm_get();                                   // get the current VMM
int32_t vmm_sync(void *vmm);                         // sync the VMM with the current VMM
int32_t vmm_switch(void *vmm);                       // switch to a different VMM

uint64_t vmm_resolve(void *vaddr); // resolve a virtual address to a physical address
uint8_t  vmm_vma(void *vaddr);     // get the VMA of the virtual address's page

#define vmm_calc(size)  (div_ceil(size, PAGE_SIZE)) // calculate min page count for a given amount of memory
#define vmm_align(size) (round_up(size, PAGE_SIZE)) // round up given size to a page size

void   *vmm_map(uint64_t num, uint64_t align, uint32_t attr); // map num amount of physically aligned pages
int32_t vmm_unmap(void *vaddr, uint64_t num, uint32_t attr); // unmap num amount of pages from the given virtual address
int32_t vmm_modify(void *vaddr, uint64_t num, uint32_t attr); // modify the attributes of num amount of pages

/*

 * map num amount of physically aligned pages, with the given attributes
 * to the given virtual address

*/
void *vmm_map_vaddr(uint64_t vaddr, uint64_t num, uint64_t align, uint32_t attr);

/*

 * map num amount of physical pages starting from the given physical address
 * to an available virtual address, with the given attributes

*/
void *vmm_map_paddr(uint64_t paddr, uint64_t num, uint32_t attr);

/*

 * map num amount of physical pages starting from the given physical address
 * to the given virtual address with the given attributes

*/
void *vmm_map_exact(uint64_t paddr, uint64_t vaddr, uint64_t num, uint32_t attr);

#endif
