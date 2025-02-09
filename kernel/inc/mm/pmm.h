#pragma once
#include "types.h"

// physical memory manager functions

int32_t pmm_init();                              // initialize PMM, required before using other PMM functions
void   *pmm_alloc(uint64_t num, uint64_t align); // allocate specific amount of pages, aligned to a specific boundry
int32_t pmm_free(void *paddr, uint64_t num);     // free specific amount of pages, starting at a given address
