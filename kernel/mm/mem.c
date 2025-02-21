#include "mm/heap.h"
#include "mm/mem.h"
#include "mm/vmm.h"

#include "util/printk.h"
#include "util/list.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

#define mem_debg(f, ...) pdebg("Mem: " f, ##__VA_ARGS__)
#define mem_fail(f, ...) pfail("Mem: " f, ##__VA_ARGS__)

uint64_t __mem_flags_from(mem_type_t type, uint64_t vma) {
  uint64_t flags = VMM_FLAGS_DEFAULT;

  switch (type) {
  case MEM_TYPE_CODE:
    break;

  case MEM_TYPE_RDONLY:
    flags |= VMM_FLAG_XD;
    flags &= ~VMM_FLAG_RW;
    break;

  case MEM_TYPE_DATA:
    flags |= VMM_FLAG_XD;
    break;

  case MEM_TYPE_STACK:
    flags |= VMM_FLAG_XD;
    break;

  case MEM_TYPE_HEAP:
    flags |= VMM_FLAG_XD;
    break;
  }

  if (VMM_VMA_USER == vma)
    flags |= VMM_FLAG_US;

  return flags;
}

#define __mem_flags(mem) (__mem_flags_from(mem->type, vmm_vma(mem->vaddr)))

mem_t *mem_new(mem_type_t type, void *vaddr, uint64_t num) {
  mem_t *new = heap_alloc(sizeof(mem_t));

  if (NULL == new) {
    mem_fail("failed to create a new memory region");
    return NULL;
  }

  bzero(new, sizeof(mem_t));
  new->type  = type;
  new->vaddr = vaddr;
  new->paddr = vmm_resolve(vaddr);
  new->num   = num;

  return new;
}

void mem_free(mem_t *mem) {
  if (NULL != mem)
    heap_free(mem);
}

mem_t *mem_del(mem_t **list, mem_type_t type, uint64_t vma) {
  slist_foreach(list, mem_t) {
    if (type != 0 && type != cur->type)
      continue;

    if (vma != 0 && vma != vmm_vma(cur->vaddr))
      continue;

    slist_del(list, cur, mem_t);
    cur->next = NULL;
    return cur;
  }

  return NULL;
}

mem_t *mem_find(mem_t **list, mem_type_t type, uint64_t vma) {
  slist_foreach(list, mem_t) {
    if (type != 0 && type != cur->type)
      continue;

    if (vma != 0 && vma != vmm_vma(cur->vaddr))
      continue;

    return cur;
  }

  return NULL;
}

int32_t mem_add(mem_t **list, mem_t *new) {
  if (NULL == list || NULL == new)
    return -EINVAL;

  if (NULL == *list) {
    *list = new;
    return 0;
  }

  /*

   * "new" can be a list, save it's next pointer
   * by inserting at the end of the list

  */
  mem_t *cur = *list;

  while (cur->next != NULL)
    cur = cur->next;
  cur->next = new;

  return 0;
}

mem_t *mem_map(mem_type_t type, uint64_t num, uint64_t vma) {
  void *vaddr = NULL, *mem = NULL;

  if ((vaddr = vmm_map_with(num, 0, vma, __mem_flags_from(type, vma))) == NULL) {
    mem_debg("failed to map %u pages in VMA 0x%p", num, vma);
    return NULL;
  }

  if ((mem = mem_new(type, vaddr, num)) == NULL) {
    vmm_unmap(vaddr, num);
    return NULL;
  }

  return mem;
}

int32_t mem_apply(mem_t *mem) {
  int32_t err = 0;

  if ((err = vmm_clear(mem->vaddr, mem->num, VMM_FLAGS_ALL)) != 0)
    return err;

  return vmm_set(mem->vaddr, mem->num, __mem_flags(mem));
}

int32_t mem_unmap(mem_t *mem) {
  if (NULL == mem)
    return -EINVAL;
  return vmm_unmap(mem->vaddr, mem->num);
}

mem_t *mem_copy(mem_t *mem) {
  if (NULL == mem)
    return NULL;

  uint64_t paddr = 0;
  void    *vaddr = NULL;

  if ((vaddr = vmm_map_to_paddr(mem->paddr, mem->num, VMM_VMA_KERNEL, VMM_FLAGS_DEFAULT)) == NULL) {
    mem_fail("failed to temporarily map physical address 0x%p", mem->paddr);
    return NULL;
  }

  if (NULL == vmm_map_to_vaddr((uint64_t)mem->vaddr, mem->num, 0, __mem_flags(mem))) {
    mem_fail("failed to map to virtual address 0x%p", mem->vaddr);
    return NULL;
  }

  memcpy(mem->vaddr, vaddr, mem->num * VMM_PAGE_SIZE);
  vmm_unmap(vaddr, mem->num);

  return mem_new(mem->type, mem->vaddr, mem->num);
}
