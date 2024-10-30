#pragma once
#include "types.h"

typedef uint64_t vmm_reg_t[2]; // defines a memory region, length and the starting address
#define vmm_reg_addr(r) (NULL == r ? 0 : r[1])
#define vmm_reg_len(r)  (NULL == r ? 0 : r[0])
#define vmm_reg_end(r)  (NULL == r ? 0 : r[1] + r[0])
#define vmm_reg_set_addr(r, a)                                                                                         \
  if (NULL != r) {                                                                                                     \
    r[1] = a;                                                                                                          \
  }
#define vmm_reg_set_len(r, l)                                                                                          \
  if (NULL != r) {                                                                                                     \
    r[0] = l;                                                                                                          \
  }
#define vmm_reg_init(r)                                                                                                \
  if (NULL != r) {                                                                                                     \
    r[0] = 0;                                                                                                          \
    r[1] = 0;                                                                                                          \
  }

// memory region functions
bool vmm_reg_clear(vmm_reg_t r); // clears a memory region
bool vmm_reg_check(vmm_reg_t r); // checks a memory region

// initializes the virtual memory manager with default max/min addresses
bool vmm_init();

// allocation functions
void *vmm_alloc(uint64_t size);
void *vmm_realloc(void *mem, uint64_t size);
void  vmm_free(void *mem);
