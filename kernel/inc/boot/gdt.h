#pragma once
#include "types.h"

// see boot/gdt.s

extern uint32_t gdt_start_addr;
extern uint32_t gdt_end_addr;

extern uint32_t gdt_desc_code_0_addr;
extern uint32_t gdt_desc_data_0_addr;
extern uint32_t gdt_desc_code_3_addr;
extern uint32_t gdt_desc_data_3_addr;

#ifndef ASM_FILE

#define gdt_offset(a) (((uint64_t)a) - ((uint64_t)gdt_start_addr))

#endif
