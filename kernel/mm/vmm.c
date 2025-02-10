#include "boot/boot.h"

#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/pm.h"

#include "util/math.h"
#include "util/mem.h"
#include "util/panic.h"
#include "util/printk.h"
#include "util/bit.h"

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
#define vmm_debg(f, ...) pdebg("VMM: " f, ##__VA_ARGS__)

#define vmm_is_valid_vma(vma) (VMM_VMA_KERNEL == vma || VMM_VMA_USER == vma)

#define vmm_entry_to_addr(entry)                                                                                       \
  ((uint64_t *)(bit_get((uint64_t)entry & VMM_FLAGS_CLEAR, 47)                                                         \
                    ? ((uint64_t)entry & VMM_FLAGS_CLEAR) | ((uint64_t)0xffff << 48)                                   \
                    : ((uint64_t)entry & VMM_FLAGS_CLEAR) & ~((uint64_t)0xffff << 48)))

#define vmm_indexes_to_addr(pml4_index, pdpt_index, pd_index, pt_index)                                                \
  ((uint64_t)pml4_index << 39 | (uint64_t)pdpt_index << 30 | (uint64_t)pd_index << 21 | (uint64_t)pt_index << 12 |     \
      (((((uint64_t)pml4_index << 39) >> 47) & 1) ? 0xffffL << 48 : 0))

#define vmm_pml4_addr()       ((uint64_t *)vmm_indexes_to_addr(510, 510, 510, 510))
#define vmm_pml4_index(vaddr) ((vaddr >> 39) & 0x1FF)
#define vmm_pml4_entry(vaddr) (vmm_pml4_addr()[vmm_pml4_index(vaddr)])

#define vmm_pdpt_addr(vaddr)  ((uint64_t *)vmm_indexes_to_addr(510, 510, 510, vmm_pml4_index(vaddr)))
#define vmm_pdpt_index(vaddr) ((vaddr >> 30) & 0x1FF)
#define vmm_pdpt_entry(vaddr) (vmm_pdpt_addr(vaddr)[vmm_pdpt_index(vaddr)])

#define vmm_pd_addr(vaddr)  ((uint64_t *)vmm_indexes_to_addr(510, 510, vmm_pml4_index(vaddr), vmm_pdpt_index(vaddr)))
#define vmm_pd_index(vaddr) ((vaddr >> 21) & 0x1FF)
#define vmm_pd_entry(vaddr) (vmm_pd_addr(vaddr)[vmm_pd_index(vaddr)])

#define vmm_pt_addr(vaddr)                                                                                             \
  ((uint64_t *)vmm_indexes_to_addr(510, vmm_pml4_index(vaddr), vmm_pdpt_index(vaddr), vmm_pd_index(vaddr)))
#define vmm_pt_index(vaddr) ((vaddr >> 12) & 0x1FF)
#define vmm_pt_entry(vaddr) (vmm_pt_addr(vaddr)[vmm_pt_index(vaddr)])

void __vmm_set_pml4(uint64_t *pml4) {
  if (NULL == pml4)
    panic("Attempt to switch to a NULL PML4");

  __asm__("mov %0, %%rax\n"
          "mov %%rax, %%cr3\n" ::"m"(pml4)
      : "%rax");
}

void *vmm_new() {
  // TODO: implement
  return NULL;
}

int32_t vmm_switch(void *vmm) {
  // TODO: implement
  return -ENOSYS;
}

void vmm_unmap(void *vaddr, uint64_t num) {
  // TODO: implement
}

bool __vmm_addr_is_mapped(uint64_t addr) {
  uint64_t pd_entry = 0;

  // check PML4, PDPT & PD entries
  if (vmm_pml4_entry(addr) == 0 || vmm_pdpt_entry(addr) == 0 || (pd_entry = vmm_pd_entry(addr)) == 0)
    return false;

  // check if the PD entry is 2 MiB page entry
  if (pd_entry & VMM_FLAG_PS)
    return true;

  // check the PT entry
  return vmm_pt_entry(addr) != 0;
}

void *__vmm_map_at_no_checks(uint64_t vaddr, uint64_t num, uint64_t align, uint64_t vma, uint64_t flags) {
  uint64_t *table = NULL, start = vaddr;
  void     *paddr = NULL;

  if (NULL == (paddr = pmm_alloc(num, align))) {
    vmm_debg("failed to allocate %u physical pages", num);
    return NULL;
  }

  vmm_debg("mapping %p to %p (%u pages, %u alignment)", paddr, vaddr, num, align);

  for (; num > 0; num--, vaddr += VMM_PAGE_SIZE, paddr += VMM_PAGE_SIZE) {
    // get (or create if not exists) PDPT
    if (*(table = &vmm_pml4_entry(vaddr)) == 0) {
      *table = (uint64_t)pmm_alloc(1, 0) | VMM_FLAGS_DEFAULT;
      bzero(vmm_pdpt_addr(vaddr), VMM_PAGE_SIZE);
    }

    if (VMM_VMA_USER == vma)
      *table |= VMM_FLAG_US;

    // get (or create if not exists) PD
    if (*(table = &vmm_pdpt_entry(vaddr)) == 0) {
      *table = (uint64_t)pmm_alloc(1, 0) | VMM_FLAGS_DEFAULT;
      bzero(vmm_pd_addr(vaddr), VMM_PAGE_SIZE);
    }

    if (VMM_VMA_USER == vma)
      *table |= VMM_FLAG_US;

    // get (or create if not exists) PT
    if (*(table = &vmm_pd_entry(vaddr)) == 0) {
      *table = (uint64_t)pmm_alloc(1, 0) | VMM_FLAGS_DEFAULT;
      bzero(vmm_pt_addr(vaddr), VMM_PAGE_SIZE);
    }

    if (VMM_VMA_USER == vma)
      *table |= VMM_FLAG_US;

    // add the page entry to PT
    table  = &vmm_pt_entry(vaddr);
    *table = (uint64_t)paddr | flags | (VMM_VMA_USER == vma ? VMM_FLAG_US : 0);
  }

  return (void *)start;
}

void *vmm_map(uint64_t num, uint64_t flags) {
  return vmm_map_with(num, 0, flags & VMM_FLAG_US ? VMM_VMA_USER : VMM_VMA_KERNEL, flags);
}

void *vmm_map_with(uint64_t num, uint64_t align, uint64_t vma, uint64_t flags) {
  if (!vmm_is_valid_vma(vma)) {
    vmm_debg("map request with an invalid VMA (0x%p)", vma);
    return NULL;
  }

  uint64_t vaddr = vma, vaddr_start = 0, cur = 0;

  for (; num > cur; vaddr += VMM_PAGE_SIZE) {
    if (cur == 0)
      vaddr_start = vaddr;

    if (VMM_VMA_USER == vma && vaddr >= VMM_VMA_USER_END)
      break;

    if (VMM_VMA_KERNEL == vma && vaddr >= VMM_VMA_KERNEL_END)
      break;

    if (__vmm_addr_is_mapped(vaddr))
      cur = 0;
    else
      cur++;
  }

  if (cur != num) {
    vmm_fail("not enough VMA in 0x%p to map %u pages", vma, num);
    return NULL;
  }

  return __vmm_map_at_no_checks(vaddr_start, num, align, vma, flags);
}

void *vmm_map_at(uint64_t vaddr, uint64_t num, uint64_t align, uint64_t vma, uint64_t flags) {
  if (!vmm_is_valid_vma(vma)) {
    vmm_debg("map request with an invalid VMA (0x%p)", vma);
    return NULL;
  }

  uint64_t vaddr_pos = vaddr, cur = 0;

  for (; num > cur; cur++, vaddr_pos += VMM_PAGE_SIZE) {
    if (VMM_VMA_USER == vma && vaddr_pos >= VMM_VMA_USER_END)
      break;

    if (VMM_VMA_KERNEL == vma && vaddr_pos >= VMM_VMA_KERNEL_END)
      break;

    if (!__vmm_addr_is_mapped(vaddr_pos))
      break;
  }

  if (cur != num) {
    vmm_fail("cannot map %u pages to 0x%p", vaddr);
    return NULL;
  }

  return __vmm_map_at_no_checks(vaddr, num, align, vma, flags);
}
