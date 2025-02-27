#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

// physical memory manager functions

int32_t  pmm_init();                              // initialize PMM, required before using other PMM functions
uint64_t pmm_alloc(uint64_t num, uint64_t align); // allocate specific amount of pages, aligned to a specific boundry
int32_t  pmm_free(uint64_t paddr, uint64_t num);  // free specific amount of pages, starting at a given address
bool     pmm_is_allocated(uint64_t paddr);        // check if the page at the given physical address is allocated

#endif
