#include "mm/vmm.h"
#include "mm/pmm.h"

#include "util/mem.h"
#include "util/printk.h"
#include "util/bit.h"
#include "util/string.h"

#include "types.h"
#include "errno.h"

bool vmm_init() {
  return false;
}

void *vmm_alloc(uint64_t size) {
  return NULL;
}

void *vmm_realloc(void *mem, uint64_t size) {
  return NULL;
}

void vmm_free(void *mem) {
  return;
}

#define vmm_fail(f, ...) pfail("VMM: " f, ##__VA_ARGS__)
#define vmm_warn(f, ...) pwarn("VMM: " f, ##__VA_ARGS__)
#define vmm_debg(f, ...) pdebg("VMM: " f, ##__VA_ARGS__)

#define vmm_vma_is_valid(vma) (VMM_VMA_KERNEL == vma || VMM_VMA_USER == vma)
#define vmm_vma_does_contain(addr)                                                                                     \
  ((VMM_VMA_KERNEL_END > addr && addr >= VMM_VMA_KERNEL) || (VMM_VMA_USER_END > addr && addr >= VMM_VMA_USER))

#define vmm_entry_to_addr(entry)                                                                                       \
  ((uint64_t)(bit_get((uint64_t)(entry) & VMM_FLAGS_CLEAR, 47)                                                         \
                  ? ((uint64_t)(entry) & VMM_FLAGS_CLEAR) | ((uint64_t)0xffff << 48)                                   \
                  : ((uint64_t)(entry) & VMM_FLAGS_CLEAR) & ~((uint64_t)0xffff << 48)))

#define vmm_indexes_to_addr(pml4_index, pdpt_index, pd_index, pt_index)                                                \
  ((uint64_t)pml4_index << 39 | (uint64_t)pdpt_index << 30 | (uint64_t)pd_index << 21 | (uint64_t)pt_index << 12 |     \
      (((((uint64_t)pml4_index << 39) >> 47) & 1) ? 0xffffL << 48 : 0))

#define vmm_pml4_vaddr()      ((uint64_t *)vmm_indexes_to_addr(510, 510, 510, 510))
#define vmm_pml4_index(vaddr) ((vaddr >> 39) & 0x1FF)
#define vmm_pml4_entry(vaddr) (vmm_pml4_vaddr()[vmm_pml4_index(vaddr)])

#define vmm_pdpt_vaddr(vaddr) ((uint64_t *)vmm_indexes_to_addr(510, 510, 510, vmm_pml4_index(vaddr)))
#define vmm_pdpt_paddr(vaddr) (vmm_entry_to_addr(vmm_pml4_entry(vaddr)))
#define vmm_pdpt_index(vaddr) ((vaddr >> 30) & 0x1FF)
#define vmm_pdpt_entry(vaddr) (vmm_pdpt_vaddr(vaddr)[vmm_pdpt_index(vaddr)])

#define vmm_pd_vaddr(vaddr) ((uint64_t *)vmm_indexes_to_addr(510, 510, vmm_pml4_index(vaddr), vmm_pdpt_index(vaddr)))
#define vmm_pd_paddr(vaddr) (vmm_entry_to_addr(vmm_pdpt_entry(vaddr)))
#define vmm_pd_index(vaddr) ((vaddr >> 21) & 0x1FF)
#define vmm_pd_entry(vaddr) (vmm_pd_vaddr(vaddr)[vmm_pd_index(vaddr)])

#define vmm_pt_vaddr(vaddr)                                                                                            \
  ((uint64_t *)vmm_indexes_to_addr(510, vmm_pml4_index(vaddr), vmm_pdpt_index(vaddr), vmm_pd_index(vaddr)))
#define vmm_pt_paddr(vaddr) (vmm_entry_to_addr(vmm_pd_entry(vaddr)))
#define vmm_pt_index(vaddr) ((vaddr >> 12) & 0x1FF)
#define vmm_pt_entry(vaddr) (vmm_pt_vaddr(vaddr)[vmm_pt_index(vaddr)])

uint64_t *__vmm_entry_from_vaddr(uint64_t vaddr) {
  uint64_t pd_entry = 0;

  // check PML4, PDPT & PD entries
  if (vmm_pml4_entry(vaddr) == 0 || vmm_pdpt_entry(vaddr) == 0 || (pd_entry = vmm_pd_entry(vaddr)) == 0)
    return NULL;

  // check if the PD entry is 2 MiB page entry
  if (pd_entry & VMM_FLAG_PS)
    return &vmm_pd_entry(vaddr);

  // check the PT entry
  return vmm_pt_entry(vaddr) == 0 ? NULL : &vmm_pt_entry(vaddr);
}

bool __vmm_is_table_free(uint64_t *table_vaddr) {
  for (uint16_t i = 0; i < VMM_TABLE_ENTRY_COUNT; i++)
    if (table_vaddr[i] != 0)
      return false;
  return true;
}

void *vmm_new() {
  uint64_t pml4_paddr, *pml4_vaddr = NULL;

  // allocate a new PML4
  if ((pml4_paddr = pmm_alloc(1, 0)) == 0) {
    vmm_warn("failed to allocate a new PML4");
    return NULL;
  }

  // temporarily map it
  if ((pml4_vaddr = vmm_map_to_paddr(pml4_paddr, 1, VMM_VMA_KERNEL, VMM_FLAGS_DEFAULT)) == NULL) {
    vmm_warn("failed to map the new PML4 @ 0x%p", pml4_paddr);
    pmm_free(pml4_paddr, 1);
    return NULL;
  }

  // copy the current PMl4's kernel VMA contents
  memcpy(pml4_vaddr + vmm_pml4_index(VMM_VMA_KERNEL),
      vmm_pml4_vaddr() + vmm_pml4_index(VMM_VMA_KERNEL),
      (VMM_TABLE_ENTRY_COUNT - vmm_pml4_index(VMM_VMA_KERNEL)) * VMM_TABLE_ENTRY_SIZE);

  // change the recursive paging entry so it point's to the new PML4
  pml4_vaddr[510] = (uint64_t)pml4_paddr | VMM_FLAGS_DEFAULT;

  // unmap it
  vmm_unmap(pml4_vaddr, 1);

  return (void *)pml4_paddr;
}

int32_t vmm_switch(void *vmm) {
  if (NULL == vmm)
    return -EINVAL;

  vmm_debg("switching to the PML4 @ 0x%p", vmm);

  __asm__("mov %0, %%rax\n"
          "mov %%rax, %%cr3\n" ::"m"(vmm)
      : "%rax");

  return 0;
}

int32_t vmm_unmap(void *_vaddr, uint64_t num) {
  uint64_t vaddr = (uint64_t)_vaddr, *entry = NULL;
  int32_t  err = 0;

  vmm_debg("unmapping %u pages from 0x%p", num, vaddr);

  for (; num > 0; num--, vaddr += VMM_PAGE_SIZE) {
    if (NULL == (entry = __vmm_entry_from_vaddr(vaddr))) {
      vmm_warn("attempt to unmap an already unmapped page (0x%p)", vaddr);
      return -EFAULT;
    }

    if ((err = pmm_free(vmm_entry_to_addr(*entry), 1)) != 0) {
      vmm_warn("failed to free the physical page @ 0x%p", vmm_entry_to_addr(*entry));
      return err;
    }

    // unmap the page from PT
    *entry = 0;

    // free & unmap the PT if it's empty
    if (__vmm_is_table_free(vmm_pt_vaddr(vaddr))) {
      if ((err = pmm_free(vmm_pt_paddr(vaddr), 1)) != 0)
        vmm_warn("failed to free PT @ 0x%p: %s", vmm_pt_paddr(vaddr), strerror(err));
      *(&vmm_pd_entry(vaddr)) = 0;
    }

    // free & unmap the PD if it's empty
    if (__vmm_is_table_free(vmm_pd_vaddr(vaddr))) {
      if ((err = pmm_free(vmm_pd_paddr(vaddr), 1)) != 0)
        vmm_warn("failed to free PD @ 0x%p: %s", vmm_pd_paddr(vaddr), strerror(err));
      *(&vmm_pdpt_entry(vaddr)) = 0;
    }

    // free & unmap the PDPT if it's empty
    if (__vmm_is_table_free(vmm_pdpt_vaddr(vaddr))) {
      if ((err = pmm_free(vmm_pdpt_paddr(vaddr), 1)) != 0)
        vmm_warn("failed to free PDPT @ 0x%p: %s", vmm_pdpt_paddr(vaddr), strerror(err));
      *(&vmm_pml4_entry(vaddr)) = 0;
    }
  }

  return 0;
}

void *__vmm_map_to_paddr_internal(uint64_t paddr, uint64_t vaddr, uint64_t num, uint64_t flags, uint64_t all_flags) {
  uint64_t *table = NULL, start = vaddr;

  vmm_debg("mapping %u pages from 0x%p to 0x%p", num, paddr, vaddr);

  for (; num > 0; num--, vaddr += VMM_PAGE_SIZE, paddr += VMM_PAGE_SIZE) {
    // get (or create if not exists) PDPT
    if (*(table = &vmm_pml4_entry(vaddr)) == 0) {
      vmm_debg("allocated a new PDPT @ 0x%p for mapping 0x%p", *table = (uint64_t)pmm_alloc(1, 0), vaddr);
      *table |= VMM_FLAGS_DEFAULT;
      bzero(vmm_pdpt_vaddr(vaddr), VMM_PAGE_SIZE);
    }

    *table |= all_flags;

    // get (or create if not exists) PD
    if (*(table = &vmm_pdpt_entry(vaddr)) == 0) {
      vmm_debg("allocated a new PD @ 0x%p for mapping 0x%p", *table = (uint64_t)pmm_alloc(1, 0), vaddr);
      *table |= VMM_FLAGS_DEFAULT;
      bzero(vmm_pd_vaddr(vaddr), VMM_PAGE_SIZE);
    }

    *table |= all_flags;

    // get (or create if not exists) PT
    if (*(table = &vmm_pd_entry(vaddr)) == 0) {
      vmm_debg("allocated a new PT @ 0x%p for mapping 0x%p", *table = (uint64_t)pmm_alloc(1, 0), vaddr);
      *table |= VMM_FLAGS_DEFAULT;
      bzero(vmm_pt_vaddr(vaddr), VMM_PAGE_SIZE);
    }

    *table |= all_flags;

    // add the page entry to PT
    table  = &vmm_pt_entry(vaddr);
    *table = (uint64_t)paddr | flags | all_flags;
  }

  return (void *)start;
}

void *__vmm_map_to_vaddr_internal(uint64_t vaddr, uint64_t num, uint64_t align, uint64_t flags) {
  uint64_t paddr = NULL;

  if (NULL == (paddr = pmm_alloc(num, align))) {
    vmm_debg("failed to allocate %u physical pages", num);
    return NULL;
  }

  return __vmm_map_to_paddr_internal(
      paddr, vaddr, num, flags, VMM_VMA_USER >= vaddr && vaddr > VMM_VMA_USER_END ? VMM_FLAG_US : 0);
}

uint64_t __vmm_find_contiguous(uint64_t num, uint64_t vma) {
  uint64_t pos = vma, start = 0, cur = 0;

  for (; num > cur; pos += VMM_PAGE_SIZE) {
    // first page, so our first allocation will be the start address
    if (cur == 0)
      start = pos;

    // make sure the virutal address is in one of the VMAs
    if (!vmm_vma_does_contain(pos))
      break;

    /*

     * if the address is mapped allocation is no longer contiguous
     * and we should find a new start address, so reset cur

    */
    if (__vmm_entry_from_vaddr(pos) != NULL)
      cur = 0;
    else
      cur++;
  }

  if (cur != num) {
    vmm_debg("not enough memory in VMA 0x%p for %u contiguous pages", vma, num);
    return 0;
  }

  return start;
}

void *vmm_map(uint64_t num, uint64_t flags) {
  return vmm_map_with(num, 0, flags & VMM_FLAG_US ? VMM_VMA_USER : VMM_VMA_KERNEL, flags);
}

void *vmm_map_with(uint64_t num, uint64_t align, uint64_t vma, uint64_t flags) {
  if (!vmm_vma_is_valid(vma)) {
    vmm_debg("map request with an invalid VMA (0x%p)", vma);
    return NULL;
  }

  uint64_t vaddr = 0;

  if ((vaddr = __vmm_find_contiguous(num, vma)) == 0)
    return NULL;

  return __vmm_map_to_vaddr_internal(vaddr, num, align, flags);
}

void *vmm_map_to_paddr(uint64_t paddr, uint64_t num, uint64_t vma, uint64_t flags) {
  if (!vmm_vma_is_valid(vma)) {
    vmm_debg("map request with an invalid VMA (0x%p)", vma);
    return NULL;
  }

  uint64_t vaddr = 0, pos = 0;

  if ((vaddr = __vmm_find_contiguous(num, vma)) == 0)
    return NULL;

  if (paddr % VMM_PAGE_SIZE != 0) {
    vmm_debg("attempt to map %u pages to an invalid physical address (0x%p)", num, paddr);
    return NULL;
  }

  for (pos = paddr; pos < paddr + num * VMM_PAGE_SIZE; pos += VMM_PAGE_SIZE) {
    if (pmm_is_allocated(pos))
      vmm_warn("mapping 0x%p, which is not a free page", pos);
  }

  return __vmm_map_to_paddr_internal(paddr, vaddr, num, flags, 0);
}

void *vmm_map_to_vaddr(uint64_t vaddr, uint64_t num, uint64_t align, uint64_t flags) {
  uint64_t vaddr_pos = vaddr, cur = 0;

  for (; num > cur; cur++, vaddr_pos += VMM_PAGE_SIZE) {
    // make sure the virutal address is in one of the VMAs
    if (!vmm_vma_does_contain(vaddr_pos))
      break;

    // we cannot map to a virutal address that is already mapped
    if (__vmm_entry_from_vaddr(vaddr_pos) != NULL)
      break;
  }

  if (cur != num) {
    vmm_fail("cannot map %u pages to 0x%p", vaddr);
    return NULL;
  }

  return __vmm_map_to_vaddr_internal(vaddr, num, align, flags);
}
