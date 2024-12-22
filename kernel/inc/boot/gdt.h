#pragma once

// see boot/gdt.s

#ifndef __ASSEMBLY__
#include "types.h"
#include "limits.h"

extern uint32_t gdt_start_addr;
extern uint32_t gdt_end_addr;

extern uint32_t gdt_desc_code_0_addr;
extern uint32_t gdt_desc_data_0_addr;
extern uint32_t gdt_desc_code_3_addr;
extern uint32_t gdt_desc_data_3_addr;
extern uint32_t gdt_desc_tss_addr;

#define gdt_offset(a) (((uint64_t)a) - ((uint64_t)gdt_start_addr))

// thank you mpetch :)
#define gdt_tss_set(ptr, limit)                                                                                        \
  do {                                                                                                                 \
    ((uint16_t *)(uint64_t)gdt_desc_tss_addr)[0] = ((uint64_t)limit) & UINT16_MAX;                                     \
    ((char *)(uint64_t)gdt_desc_tss_addr)[6]     = (((uint64_t)limit) >> 16) & 0b1111;                                 \
                                                                                                                       \
    ((uint16_t *)(uint64_t)gdt_desc_tss_addr)[1] = ((uint64_t)ptr) & UINT16_MAX;                                       \
    ((char *)(uint64_t)gdt_desc_tss_addr)[4]     = (((uint64_t)ptr) >> 16) & UINT8_MAX;                                \
    ((char *)(uint64_t)gdt_desc_tss_addr)[7]     = (((uint64_t)ptr) >> 24) & UINT8_MAX;                                \
    ((uint32_t *)(uint64_t)gdt_desc_tss_addr)[2] = (((uint64_t)ptr) >> 32) & UINT32_MAX;                               \
  } while (0);

#endif
