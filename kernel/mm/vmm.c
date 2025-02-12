#include "boot/boot.h"

#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/pm.h"

#include "util/math.h"
#include "util/mem.h"
#include "util/panic.h"
#include "util/printk.h"
#include "util/bit.h"
#include "util/string.h"

#include "types.h"
#include "errno.h"

bool vmm_reg_check(vmm_reg_t r) {
  if (NULL == r)
    return false;

  if (vmm_reg_len(r) <= 0)
    return false;

  return true;
}

bool vmm_reg_clear(vmm_reg_t r) {
  if (!vmm_reg_check(r))
    return false;

  for (uint64_t i = 0; i < vmm_reg_len(r); i++)
    ((char *)vmm_reg_addr(r))[i] = 0;

  return true;
}

struct vmm_header {
  uint16_t magic   : 15; // stores the magic value (signature)
  uint8_t  is_free : 1;  // stores if the region described by the header is free or not
  uint64_t size;         // stores the size of the regiona
} __attribute__((packed));

#define VMM_HEADER_FREE  1
#define VMM_HEADER_USED  0
#define VMM_HEADER_MAGIC 20249 // the 15 bit magic value for the header
//
#define vmm_header_verify(h) (h->magic == VMM_HEADER_MAGIC) // checks a header pointer using the magic value
#define vmm_header_size()    sizeof(struct vmm_header)      // returns the size of the header
#define vmm_header(p)        ((struct vmm_header *)(p))     // converts a value to vmm_header pointer

struct vmm_state {
  uint64_t start; // start address of the currently allocated page
  uint64_t end;   // end address of the currently allocated page
  uint64_t cur;   // start address of the currently available memory
} vmm_st;

bool vmm_init() {
  bzero(&vmm_st, sizeof(vmm_st));
  return true;
}

// allocates "size" amounts of memory
void *vmm_alloc(uint64_t size) {
  // minimum size is 1
  if (size == 0)
    return NULL;

  uint64_t available = vmm_st.cur - vmm_st.start; // available memory size
  uint64_t old_start = vmm_st.start;              // save the start address

  uint64_t required_pages = div_ceil(size, PM_PAGE_SIZE);             // required page count for the allocation
  uint64_t min_pages      = div_ceil(size - available, PM_PAGE_SIZE); // min page count for the allocation

  // lets see if a have any available page(s)
  if (vmm_st.start == 0 && vmm_st.end == 0) {
    // if not let's allocate enough pages
  new_pages:
    if ((vmm_st.start = (uint64_t)pm_alloc(required_pages)) == 0) {
      printk(KERN_FAIL, "VMM: cannot allocate %l memory, failed to allocate %l pages", size, required_pages);
      return NULL;
    }
    vmm_st.cur = vmm_st.end = vmm_st.start + (required_pages * PM_PAGE_SIZE);
    goto alloc;
  }

  // is the available memory enough for the size?
  if (available >= size)
    goto alloc;

  // if not lets allocate more pages for the rest of the size
  if ((vmm_st.start = (uint64_t)pm_alloc(min_pages)) == 0) {
    vmm_st.start = old_start; // restore the old start address
    printk(KERN_FAIL, "VMM: cannot allocate %l memory, failed to allocate %l pages", size, min_pages);
    return NULL;
  }

  // if the allocated pages are not contiguous with the current allocated pages, allocate completely new ones
  if (old_start - (min_pages * PM_PAGE_SIZE) != vmm_st.start) {
    pm_free((void *)vmm_st.start, min_pages);
    goto new_pages;
  }

alloc:
  // cur now points to the start of the header
  vmm_st.cur -= size + vmm_header_size();

  // initialize the header
  vmm_header(vmm_st.cur)->is_free = VMM_HEADER_USED;
  vmm_header(vmm_st.cur)->magic   = VMM_HEADER_MAGIC;
  vmm_header(vmm_st.cur)->size    = size;

  // return the allocated memory pointer
  return (void *)(vmm_st.cur + vmm_header_size());
}

bool __vm_free_is_page_free(void *page) {
  struct vmm_header *h = vmm_header(page);
  while (vmm_header_verify(h) && h != page + PM_PAGE_SIZE) {
    if (h->is_free != VMM_HEADER_FREE)
      return false;
    // move onto next header
    h += vmm_header_size();
    h += h->size;
  }
  return true;
}

void __vm_free_rewind() {
  if (vmm_st.cur == vmm_st.end) {
    if (!__vm_free_is_page_free((void *)(vmm_st.end - PM_PAGE_SIZE)))
      return;

    vmm_st.end -= PM_PAGE_SIZE;
    pm_free((void *)vmm_st.end, 1);

    if (vmm_st.end == vmm_st.start)
      bzero(&vmm_st, sizeof(vmm_st));
    return;
  }

  struct vmm_header *h = vmm_header(vmm_st.cur);

  if (!vmm_header_verify(h))
    return panic("Allocated VMM region headers are corrupted");

  if (!h->is_free)
    return;

  vmm_st.cur += vmm_header_size();
  vmm_st.cur += h->size;

  // recursive call to rewind back as far as possible
  __vm_free_rewind();
}

void vmm_free(void *mem) {
  // make sure the pointer points to a valid memory address
  if (NULL == mem)
    return;

  struct vmm_header *h = vmm_header(mem - vmm_header_size());

  // if provided memory address does not contain a header,
  // we can just return as the address does not point to a region
  if (!vmm_header_verify(h) || h->is_free == VMM_HEADER_FREE)
    return;

  h->is_free = VMM_HEADER_FREE;

  if ((uint64_t)h == vmm_st.cur) {
    vmm_st.cur += vmm_header_size();
    vmm_st.cur += h->size;
  }

  __vm_free_rewind();
}

void *__vmm_realloc_cur_shrink(struct vmm_header *h, uint64_t size) {
  uint64_t diff = h->size - size, ti = 0;
  uint64_t new_size = size + vmm_header_size();
  char     temp[diff];

  vmm_st.cur += diff;

  for (uint64_t i = 0; i < new_size; i++) {
    if (ti >= diff)
      ti = 0;

    if (i < diff) {
      temp[ti++]              = ((char *)vmm_st.cur)[i];
      ((char *)vmm_st.cur)[i] = ((char *)h)[i];
      continue;
    }

    memswap(&((char *)vmm_st.cur)[i], &temp[ti++]);
  }

  // change region size in the header
  vmm_header(vmm_st.cur)->size -= diff;

  return (void *)(vmm_st.cur + vmm_header_size());
}

void *__vmm_realloc_cur(struct vmm_header *h, uint64_t size) {
  if (size < h->size)
    return __vmm_realloc_cur_shrink(h, size);

  uint64_t available = vmm_st.cur - vmm_st.start; // available memory size
  uint64_t old_size  = h->size + vmm_header_size();
  uint64_t diff      = size - h->size;

  // is the memory we have is enough for the reallocation?
  if (available >= diff)
    goto realloc;

  uint64_t old_start  = vmm_st.start;
  uint64_t page_count = div_ceil(diff, PM_PAGE_SIZE);

  // if not lets we can allocate more pages for the rest of the size
  if ((vmm_st.start = (uint64_t)pm_alloc(page_count)) == 0) {
    vmm_st.start = old_start; // restore the old start address
    printk(KERN_FAIL, "VMM: cannot allocate %l more memory, failed to allocate %l new pages", diff, page_count);
    return NULL;
  }

  // if the allocated pages are not contiguous with the current allocated pages, we might just reallocate
  // the whole entire thing
  if (old_start - (page_count * PM_PAGE_SIZE) != vmm_st.start) {
    pm_free((void *)vmm_st.start, page_count);
    return NULL;
  }

realloc:
  vmm_st.cur -= diff;

  for (uint64_t i = 0; i < old_size; i++)
    ((char *)vmm_st.cur)[i] = ((char *)h)[i];

  // expand the size defined in the header
  vmm_header(vmm_st.cur)->size += diff;

  return (void *)(vmm_st.cur + vmm_header_size());
}

void *vmm_realloc(void *mem, uint64_t size) {
  // minimum size is 1
  if (size == 0)
    return NULL;

  // we need a valid memory address, if it's NULL just use vmm_alloc to allocate new memory
  if (NULL == mem)
    return vmm_alloc(size);

  // stores the allocation header for "mem"
  struct vmm_header *h = vmm_header(mem - vmm_header_size());

  // check if the provided memory address contains a valid
  // allocated memory region header
  if (!vmm_header_verify(h) || h->is_free == VMM_HEADER_FREE)
    return NULL;

  // sir why the fuck did you do that
  if (h->size == size)
    return mem;

  /*

   * we might be able to just expand the region if we are currently
   * pointing to the allocated region, if this works then we will use the same region
   * so we return the size however the size would be expanded

  */
  if ((uint64_t)h == vmm_st.cur) {
    void *cur_res = __vmm_realloc_cur(h, size);
    if (NULL != cur_res)
      return cur_res;
  }

  // stores the amount of memory that will be copied to new memory
  uint64_t copy_size = size > h->size ? h->size : size;
  void    *new_mem   = NULL; // new memory
  uint64_t i         = 0;    // counter

  if ((new_mem = vmm_alloc(size)) == NULL) {
    printk(KERN_FAIL, "VMM: failed to reallocate memory region %p:%l", mem, size);
    return NULL;
  }

  for (; i < copy_size; i++)
    ((char *)new_mem)[i] = ((char *)mem)[i];

  vmm_free(mem);
  return new_mem;
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

void __vmm_set_pml4(uint64_t *pml4) {
  if (NULL == pml4)
    panic("Attempt to switch to a NULL PML4");

  __asm__("mov %0, %%rax\n"
          "mov %%rax, %%cr3\n" ::"m"(pml4)
      : "%rax");
}

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
  // TODO: implement
  return NULL;
}

int32_t vmm_switch(void *vmm) {
  // TODO: implement
  return -ENOSYS;
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
