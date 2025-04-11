#include "sched/sched.h"
#include "sched/task.h"

#include "mm/paging.h"
#include "mm/vmm.h"
#include "mm/pmm.h"

#include "util/string.h"
#include "util/printk.h"
#include "util/mem.h"
#include "util/bit.h"
#include "util/asm.h"

#include "types.h"
#include "errno.h"

#define vmm_fail(f, ...) pfail("VMM: " f, ##__VA_ARGS__)
#define vmm_warn(f, ...) pwarn("VMM: " f, ##__VA_ARGS__)
#define vmm_debg(f, ...) pdebg("VMM: " f, ##__VA_ARGS__)

// virtual memory areas
#define VMM_VMA_USER_START (0x0000000000000000 + PAGE_SIZE) // 0x0 can be interpreted as NULL
#define VMM_VMA_USER_END   (0x00007fffffffffff)

#define VMM_VMA_KERNEL_START (0xffff800000000000)
#define VMM_VMA_KERNEL_END   (0xffffffffffffffff)

// some helper macros
#define vmm_addr_is_valid(addr)                                                                                        \
  ((BOOT_KERNEL_START_VADDR > addr && addr >= VMM_VMA_KERNEL_START) ||                                                 \
      (VMM_VMA_USER_END > addr && addr >= VMM_VMA_USER_START))
#define vmm_entry_to_addr(entry)                                                                                       \
  ((uint64_t)(bit_get((uint64_t)(entry) & PTE_FLAGS_CLEAR, 47)                                                         \
                  ? ((uint64_t)(entry) & PTE_FLAGS_CLEAR) | ((uint64_t)0xffff << 48)                                   \
                  : ((uint64_t)(entry) & PTE_FLAGS_CLEAR) & ~((uint64_t)0xffff << 48)))

#define vmm_entry_to_flags(entry) (((uint64_t)(entry) & ~(PTE_FLAGS_CLEAR)) & ~(PTE_FLAG_A | PTE_FLAG_D))

#define vmm_indexes_to_addr(pml4_index, pdpt_index, pd_index, pt_index)                                                \
  ((uint64_t)pml4_index << 39 | (uint64_t)pdpt_index << 30 | (uint64_t)pd_index << 21 | (uint64_t)pt_index << 12 |     \
      (((((uint64_t)pml4_index << 39) >> 47) & 1) ? 0xffffL << 48 : 0))

#define vmm_invlpg(vaddr) __asm__("invlpg %0\n" ::"m"(*(uint64_t *)vaddr))

// table macros
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
  if (pd_entry & PTE_FLAG_PS)
    return &vmm_pd_entry(vaddr);

  // check the PT entry
  return vmm_pt_entry(vaddr) == 0 ? NULL : &vmm_pt_entry(vaddr);
}

uint64_t __vmm_attr_to_flags(uint32_t attr, bool for_page_table) {
  uint64_t flags = PTE_FLAGS_DEFAULT;

  if (attr & VMM_ATTR_USER)
    flags |= PTE_FLAG_US;

  if (for_page_table)
    return flags;

  if (!(attr & VMM_ATTR_SAVE))
    flags |= PTE_FLAG_PMM;

  if (attr & VMM_ATTR_NO_EXEC)
    flags |= PTE_FLAG_XD;

  if (attr & VMM_ATTR_NO_CACHE)
    flags |= PTE_FLAG_PCD;

  if (attr & VMM_ATTR_RDONLY)
    flags &= ~(PTE_FLAG_RW);

  return flags;
}

bool __vmm_is_table_free(uint64_t *table_vaddr) {
  for (uint16_t i = 0; i < PTE_COUNT; i++)
    if (table_vaddr[i] != 0)
      return false;
  return true;
}

int32_t __vmm_alert_tasks() {
  void   *vmm  = vmm_get();
  task_t *task = NULL;

  while (NULL != (task = sched_next(task))) {
    if (vmm != task->vmm)
      task->old = true;
  }

  return 0;
}

int32_t vmm_init() {
  /*

   * enable the XD page flag (bit 11 on EFER)

   * for more information see:
   * https://wiki.osdev.org/Paging#Page_Map_Table_Entries
   * https://wiki.osdev.org/CPU_Registers_x86-64#IA32_EFER

  */
  uint64_t efer = _msr_read(MSR_EFER);
  _msr_write(MSR_EFER, efer | (1 << 11));

  return 0;
}

int32_t vmm_sync(void *vmm) {
  uint64_t pml4_paddr = (uint64_t)vmm, *pml4_vaddr = NULL;
  vmm_debg("syncing PML4 @ 0x%p", pml4_paddr);

  // temporarily map old PML4
  if ((pml4_vaddr = vmm_map_paddr(pml4_paddr, 1, VMM_ATTR_SAVE)) == NULL) {
    vmm_warn("failed to map the old PML4 @ 0x%p to sync", pml4_paddr);
    return -EFAULT;
  }

  // clean the user VMA contents
  bzero(pml4_vaddr, vmm_pml4_index(VMM_VMA_USER_END) * PTE_SIZE);

  // copy the current PML4's kernel VMA contents
  memcpy(pml4_vaddr + vmm_pml4_index(VMM_VMA_KERNEL_START),
      vmm_pml4_vaddr() + vmm_pml4_index(VMM_VMA_KERNEL_START),
      (PTE_COUNT - vmm_pml4_index(VMM_VMA_KERNEL_START)) * PTE_SIZE);

  // fix the recursive paging entry
  pml4_vaddr[510] = (uint64_t)pml4_paddr | PTE_FLAGS_DEFAULT;

  // unmap the PML4
  return vmm_unmap(pml4_vaddr, 1, VMM_ATTR_SAVE);
}

void *vmm_new() {
  uint64_t pml4_paddr = 0;

  // allocate a new PML4
  if ((pml4_paddr = pmm_alloc(1, 0)) == 0) {
    vmm_warn("failed to allocate a new PML4");
    return NULL;
  }

  // sync the kernel VMA with the current one
  if (vmm_sync((void *)pml4_paddr) != 0) {
    vmm_warn("failed to sync new PML4 @ 0x%p", pml4_paddr);
    pmm_free(pml4_paddr, 1);
    return NULL;
  }

  return (void *)pml4_paddr;
}

void *vmm_get() {
  void *vmm;

  __asm__("mov %%cr3, %%rax\n"
          "mov %%rax, %0\n" ::"m"(vmm)
      : "%rax");

  return vmm;
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

int32_t vmm_set(void *vaddr, uint64_t num, uint64_t flags) {
  uint64_t *entry = NULL;

  if (NULL == (entry = __vmm_entry_from_vaddr((uint64_t)vaddr)))
    return -EFAULT;

  *entry |= flags;
  return 0;
}

int32_t vmm_clear(void *vaddr, uint64_t num, uint64_t flags) {
  uint64_t *entry = NULL;

  if (NULL == (entry = __vmm_entry_from_vaddr((uint64_t)vaddr)))
    return -EFAULT;

  *entry &= ~(flags);
  return 0;
}

uint8_t vmm_vma(void *vaddr) {
  if (VMM_VMA_KERNEL_END > (uint64_t)vaddr && (uint64_t)vaddr >= VMM_VMA_KERNEL_START)
    return VMM_VMA_KERNEL;
  return VMM_VMA_USER;
}

uint64_t vmm_resolve(void *vaddr) {
  uint64_t *entry = NULL;

  if (NULL == (entry = __vmm_entry_from_vaddr((uint64_t)vaddr)))
    return 0;

  if (*entry & PTE_FLAG_PS)
    return vmm_entry_to_addr(*entry) | ((uint64_t)vaddr & 0x1fffff);

  return vmm_entry_to_addr(*entry) | ((uint64_t)vaddr & 0xfff);
}

int32_t vmm_unmap(void *_vaddr, uint64_t num, uint32_t attr) {
  uint64_t vaddr = (uint64_t)_vaddr, *entry = NULL;
  int32_t  err = 0;

  vmm_debg("unmapping %u pages from 0x%p", num, vaddr);

  for (; num > 0; num--, vaddr += PAGE_SIZE) {
    if (NULL == (entry = __vmm_entry_from_vaddr(vaddr))) {
      vmm_warn("attempt to unmap an already unmapped page (0x%p)", vaddr);
      return -EFAULT;
    }

    if (*entry & PTE_FLAG_PMM && !(attr & VMM_ATTR_SAVE) && (err = pmm_free(vmm_entry_to_addr(*entry), 1)) != 0) {
      vmm_warn("failed to free the physical page @ 0x%p", vmm_entry_to_addr(*entry));
      return err;
    }

    // unmap the page from PT
    *entry = 0;
    vmm_invlpg(vaddr);

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

      // we modified PML4, other tasks should sync before switching
      __vmm_alert_tasks();
    }
  }

  return 0;
}

void *__vmm_map_to_paddr_internal(uint64_t paddr, uint64_t vaddr, uint64_t num, uint32_t attr) {
  uint64_t *entry = NULL, start = vaddr, flags = __vmm_attr_to_flags(attr, true);
  bool      invalidate = false;
  int32_t   err        = 0;

  vmm_debg("mapping %u pages from 0x%p to 0x%p", num, paddr, vaddr);

  for (; num > 0; num--, vaddr += PAGE_SIZE, paddr += PAGE_SIZE) {
    // get (or create if not exists) PDPT
    if (*(entry = &vmm_pml4_entry(vaddr)) == 0) {
      vmm_debg("allocated a new PDPT @ 0x%p for mapping 0x%p", *entry = pmm_alloc(1, 0), vaddr);
      *entry |= flags;
      __vmm_alert_tasks();
      bzero(vmm_pdpt_vaddr(vaddr), PAGE_SIZE);
    }

    // update the flags of the entry
    else {
      if (vmm_entry_to_flags(*entry) != flags)
        __vmm_alert_tasks(); // we modified PML4, other tasks should sync
      *entry |= flags;
    }

    // get (or create if not exists) PD
    if (*(entry = &vmm_pdpt_entry(vaddr)) == 0) {
      vmm_debg("allocated a new PD @ 0x%p for mapping 0x%p", *entry = pmm_alloc(1, 0), vaddr);
      *entry |= flags;
      bzero(vmm_pd_vaddr(vaddr), PAGE_SIZE);
    }

    // update the flags of the entry
    else
      *entry |= flags;

    // get (or create if not exists) PT
    if (*(entry = &vmm_pd_entry(vaddr)) == 0) {
      vmm_debg("allocated a new PT @ 0x%p for mapping 0x%p", *entry = pmm_alloc(1, 0), vaddr);
      *entry |= flags;
      bzero(vmm_pt_vaddr(vaddr), PAGE_SIZE);
    }

    // update the flags of the entry
    else
      *entry |= flags;

    // get the page entry from PT
    entry      = &vmm_pt_entry(vaddr);
    invalidate = *entry != 0;

    // add the entry with the flags
    *entry = paddr | __vmm_attr_to_flags(attr, false);

    // invalidate TLB cache for the page if vaddr was already mapped
    if (invalidate)
      vmm_invlpg(vaddr);
  }

  return (void *)start;
}

void *__vmm_map_to_vaddr_internal(uint64_t vaddr, uint64_t num, uint64_t align, uint32_t attr) {
  uint64_t paddr = NULL;

  if (NULL == (paddr = pmm_alloc(num, align))) {
    vmm_debg("failed to allocate %u physical pages", num);
    return NULL;
  }

  return __vmm_map_to_paddr_internal(paddr, vaddr, num, attr);
}

uint64_t __vmm_find_contiguous(uint64_t num, uint64_t align, uint32_t attr) {
  uint64_t pos = 0, start = 0, cur = 0;

  if (attr & VMM_ATTR_USER)
    pos = VMM_VMA_USER_START;
  else
    pos = VMM_VMA_KERNEL_START;

  for (; num > cur; pos += PAGE_SIZE) {
    // first page, so our first allocation will be the start address
    if (cur == 0) {
      // make sure the start address is aligned
      if (align != 0 && pos % align != 0)
        continue;

      // set the start address
      start = pos;
    }

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
    vmm_debg("not enough memory for %u contiguous pages", num);
    return 0;
  }

  return start;
}

void *vmm_map(uint64_t num, uint64_t align, uint32_t attr) {
  uint64_t vaddr = 0;

  if ((vaddr = __vmm_find_contiguous(num, align, attr)) == 0)
    return NULL;

  return __vmm_map_to_vaddr_internal(vaddr, num, align, attr);
}

void *vmm_map_paddr(uint64_t paddr, uint64_t num, uint32_t attr) {
  uint64_t vaddr = 0, pos = 0;

  if ((vaddr = __vmm_find_contiguous(num, 0, attr)) == 0)
    return NULL;

  if (paddr % PAGE_SIZE != 0) {
    vmm_debg("attempt to map %u pages to an invalid physical address (0x%p)", num, paddr);
    return NULL;
  }

  /*for (pos = paddr; pos < paddr + num * VMM_PAGE_SIZE; pos += VMM_PAGE_SIZE) {
    if (pmm_is_allocated(pos))
      vmm_debg("mapping paddr 0x%p, which is not a free page", pos);
  }*/

  return __vmm_map_to_paddr_internal(paddr, vaddr, num, attr);
}

void *vmm_map_vaddr(uint64_t vaddr, uint64_t num, uint64_t align, uint32_t attr) {
  uint64_t vaddr_pos = vaddr, cur = 0;

  for (; num > cur; cur++, vaddr_pos += PAGE_SIZE) {
    // make sure the virutal address is in one of the VMAs
    if (!vmm_addr_is_valid(vaddr_pos))
      break;

    /*

     * we cannot map to a virutal address that is already mapped
     * unless REUSE attribute is used

    */
    if (!(attr & VMM_ATTR_REUSE) && __vmm_entry_from_vaddr(vaddr_pos) != NULL)
      break;
  }

  if (cur != num) {
    vmm_fail("cannot map %u pages to 0x%p", num, vaddr);
    return NULL;
  }

  return __vmm_map_to_vaddr_internal(vaddr, num, align, attr);
}

void *vmm_map_exact(uint64_t paddr, uint64_t vaddr, uint64_t num, uint32_t attr) {
  uint64_t  vaddr_pos = vaddr, paddr_pos = paddr, cur = 0;
  uint64_t *entry = NULL;

  for (; num > cur; cur++, vaddr_pos += PAGE_SIZE, paddr_pos += PAGE_SIZE) {
    // make sure the virtual address is valid
    if (!vmm_addr_is_valid(vaddr_pos))
      break;

    // no entry? page is available, continue
    if ((entry = __vmm_entry_from_vaddr(vaddr_pos)) == NULL)
      continue;

    /*

     * check if vaddr is already mapped to the paddr

     * if so move to the next page, and increment vaddr,
     * and decrement num, so we don't waste time mapping
     * it again

    */
    if (cur == 0 && entry != NULL && vmm_entry_to_addr(*entry) == paddr_pos) {
      vaddr += PAGE_SIZE;
      paddr += PAGE_SIZE;
      num--;
    }

    // if REUSE is not set, we cannot remap an already mapped vaddr
    if (!(attr & VMM_ATTR_REUSE) && entry != NULL)
      break;
  }

  if (cur != num) {
    vmm_fail("cannot map %u pages from 0x%p to 0x%p", num, paddr, vaddr);
    return NULL;
  }

  return __vmm_map_to_paddr_internal(paddr, vaddr, num, attr);
}
