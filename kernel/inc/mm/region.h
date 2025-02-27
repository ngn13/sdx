#pragma once
#include "util/list.h"
#include "types.h"

/*

 * memory region types, each type have unique use
 * and they use different VMM attributes

 * not using enums because they are cringe and i
 * cannot access them from ASM

*/

#define REGION_TYPE_CODE   (1) // memory region contains runable code
#define REGION_TYPE_RDONLY (2) // memory region contains read-only data
#define REGION_TYPE_DATA   (3) // memory region contains read/write data
#define REGION_TYPE_HEAP   (4) // memory region contains heap memory
#define REGION_TYPE_STACK  (5) // memory region contains program stack

#ifndef __ASSEMBLY__

// describes a memory region
typedef struct region {
  uint8_t        type;  // memory region type (what it's used for)
  uint8_t        vma;   // memory region VMA
  void          *vaddr; // memory region virtual start address
  uint64_t       paddr; // memory region physical start address
  uint64_t       num;   // number of pages in the region
  struct region *next;  // next memory region
} region_t;

region_t   *region_new(uint8_t type, uint8_t vma, void *vaddr, uint64_t num); // create a new memory region
const char *region_name(region_t *mem);                                       // get the name of the region
region_t   *region_copy(region_t *mem);                                       // copy a memory region with it's contents
void        region_free(region_t *mem);                                       // free the memory region

#define region_each(list) slist_foreach(list, region_t)
region_t *region_find(region_t **head, uint8_t type, uint8_t vma); // find a memory region in a memory region list
int32_t   region_del(region_t **head, region_t *mem);              // delete a memory region from a memory region list
int32_t   region_add(region_t **head, region_t *mem);              // add a memory region to the memory region list

int32_t region_map(region_t *mem);   // map the provided memory region
int32_t region_unmap(region_t *mem); // unmap the provided memory region

#endif
