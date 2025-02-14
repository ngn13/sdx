#pragma once

#include "types.h"
#include "limits.h"

#define BOOT_KERNEL_ADDR 0xffffffff80000000
#define BOOT_STACK_SIZE  0x10000

#ifndef __ASSEMBLY__

#include "util/asm.h"

#define BOOT_KERNEL_START_PADDR (_start_addr - BOOT_KERNEL_ADDR)
#define BOOT_KERNEL_END_PADDR   (_end_addr - BOOT_KERNEL_ADDR)

#define BOOT_KERNEL_START_VADDR (_start_addr)
#define BOOT_KERNEL_END_VADDR   (_end_addr)

// boot/paging.S
extern uint32_t paging_mb_data_offset;
#define BOOT_MB_DATA_ADDR (0xffffffff80200000 + paging_mb_data_offset)

extern uint64_t paging_temp_tables_addr;
#define BOOT_TEMP_PML4_VADDR (paging_temp_tables_addr)
#define BOOT_TEMP_PML4_PADDR (paging_temp_tables_addr - BOOT_KERNEL_ADDR)

// boot/gdt.S
extern uint64_t gdt_start_addr;
extern uint64_t gdt_end_addr;

extern uint64_t gdt_desc_kernel_code_addr;
extern uint64_t gdt_desc_kernel_data_addr;
extern uint64_t gdt_desc_user_code_addr;
extern uint64_t gdt_desc_user_data_addr;
extern uint64_t gdt_desc_tss_addr;

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
