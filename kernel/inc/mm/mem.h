#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

// memory region types
typedef enum {
  MEM_TYPE_CODE = 1, // memory region contains runable code
  MEM_TYPE_RDONLY,   // memory region contains read-only data
  MEM_TYPE_DATA,     // memory region contains read/write data
  MEM_TYPE_STACK,    // memory region contains program stack
  MEM_TYPE_HEAP,     // memory region contains heap memory
} mem_type_t;

// describes a memory region
typedef struct mem {
  mem_type_t  type;  // memory region type (what it's used for)
  void       *vaddr; // memory region virtual start address
  uint64_t    paddr; // memory region physical start address
  uint64_t    num;   // number of pages in the region
  struct mem *next;  // next memory region
} mem_t;

mem_t *mem_new(mem_type_t type, void *vaddr, uint64_t num); // create a new memory region
void   mem_free(mem_t *mem);                                // free the memory region

mem_t  *mem_del(mem_t **list, mem_type_t type, uint64_t vma); // delete & free a memory region from a memory region list
mem_t  *mem_find(mem_t **list, mem_type_t type, uint64_t vma); // find a memory region in a memory region list
int32_t mem_add(mem_t **list, mem_t *new);                     // add a memory region to the memory region list

mem_t  *mem_map(mem_type_t type, uint64_t num, uint64_t vma); // map & create a new memory region
int32_t mem_apply(mem_t *mem);                                // apply required changes to the give memory region
int32_t mem_unmap(mem_t *mem);                                // unmap a memory region
mem_t  *mem_copy(mem_t *mem);                                 // copy a memory region

#endif
