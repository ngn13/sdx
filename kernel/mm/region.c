#include "mm/region.h"
#include "mm/heap.h"
#include "mm/vmm.h"

#include "util/printk.h"
#include "util/list.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

struct region_type_data {
  uint8_t     type;
  const char *name;
  uint32_t    attr;
};

struct region_type_data region_type_data[] = {
    {REGION_TYPE_CODE,   "CODE",      0               },
    {REGION_TYPE_RDONLY, "READ_ONLY", VMM_ATTR_RDONLY },
    {REGION_TYPE_DATA,   "DATA",      VMM_ATTR_NO_EXEC},
    {REGION_TYPE_HEAP,   "HEAP",      VMM_ATTR_NO_EXEC},
    {REGION_TYPE_STACK,  "STACK",     VMM_ATTR_NO_EXEC},
};

#define __region_attr(type) (region_type_data[type - 1].attr | VMM_ATTR_REUSE)
#define __region_name(type) (region_type_data[type - 1].name)

region_t *region_new(uint8_t type, uint8_t vma, void *vaddr, uint64_t num) {
  region_t *new = heap_alloc(sizeof(region_t));

  if (NULL == new)
    return NULL;

  bzero(new, sizeof(region_t));
  new->type  = type;
  new->vma   = vma;
  new->vaddr = vaddr;
  new->num   = num;

  return new;
}

void region_free(region_t *mem) {
  if (NULL == mem)
    return;

  /*

   * if we previously mapped the region, free the physical
   * memory it's using, as vmm_unmap() doesn't do that since
   * it uses vmm_unmap with SAVE attribute

  */
  if (mem->paddr != 0)
    pmm_free(mem->paddr, mem->num);

  // free the memory region object
  heap_free(mem);
}

const char *region_name(region_t *mem) {
  return __region_name(mem->type);
}

int32_t region_del(region_t **head, region_t *mem) {
  region_t *cur = NULL, *pre = NULL;

  if (NULL == head || NULL == mem)
    return -EINVAL;

  for (cur = *head; cur != NULL; pre = cur, cur = cur->next) {
    if (cur != mem)
      continue;

    if (NULL == pre)
      *head = cur->next;
    else
      pre->next = cur->next;

    break;
  }

  return 0;
}

int32_t region_add(region_t **head, region_t *new) {
  if (NULL == head || NULL == new)
    return -EINVAL;

  if (NULL == *head) {
    *head = new;
    return 0;
  }

  /*

   * "new" can be a list, save it's next pointer
   * by inserting at the end of the list

  */
  region_t *cur = *head;

  while (cur->next != NULL)
    cur = cur->next;
  cur->next = new;

  return 0;
}

region_t *region_find(region_t **head, uint8_t type, uint8_t vma) {
  slist_foreach(head, region_t) {
    if (type != 0 && type != cur->type)
      continue;

    if (cur->vma == vma)
      return cur;
  }

  return NULL;
}

int32_t region_map(region_t *mem) {
  if (NULL == mem)
    return -EINVAL;

  void    *vaddr = NULL;
  uint64_t attr  = 0;

  // setup the VMM attributes for the memory region
  attr |= __region_attr(mem->type) | VMM_ATTR_SAVE;
  attr |= (mem->vma == VMM_VMA_USER ? VMM_ATTR_USER : 0);

  // if vaddr is NULL, use vmm_map() to get a free vaddr
  if (NULL == mem->vaddr) {
    vaddr = mem->vaddr = vmm_map(mem->num, 0, attr);
    mem->paddr         = vmm_resolve(mem->vaddr);
  }

  // if paddr is NULL, map the specified vaddr to a free paddr
  else if (mem->paddr == 0) {
    vaddr      = vmm_map_vaddr((uint64_t)mem->vaddr, mem->num, 0, attr);
    mem->paddr = vmm_resolve(vaddr);
  }

  // if we already have vaddr and paddr, just map the paddr to exact vaddr again
  else
    vaddr = vmm_map_exact(mem->paddr, (uint64_t)mem->vaddr, mem->num, attr);

  // check the result of the mapping
  return NULL == vaddr ? -EFAULT : 0;
}

int32_t region_unmap(region_t *mem) {
  if (NULL == mem)
    return -EINVAL;
  return vmm_unmap(mem->vaddr, mem->num, VMM_ATTR_SAVE);
}

region_t *region_copy(region_t *mem) {
  if (NULL == mem)
    return NULL;

  void     *vaddr = NULL;
  region_t *copy  = NULL;

  if ((vaddr = vmm_map(mem->num, 0, 0)) == NULL)
    goto end;

  if ((copy = heap_alloc(sizeof(region_t))) == NULL)
    goto end;

  // copy the original region to the new one
  memcpy(vaddr, mem->vaddr, mem->num * PAGE_SIZE);

  // setup the new copied region structure
  bzero(copy, sizeof(region_t));
  copy->type  = mem->type;
  copy->vaddr = mem->vaddr;
  copy->vma   = mem->vma;
  copy->paddr = vmm_resolve(vaddr);
  copy->num   = mem->num;

end:
  if (NULL != vaddr)
    vmm_unmap(vaddr, mem->num, VMM_ATTR_SAVE);

  return copy;
}
