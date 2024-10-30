#pragma once
#include "types.h"

// see boot/gdt.s

extern uint64_t gdt_start_addr;
extern uint64_t gdt_end_addr;

extern uint64_t gdt_desc_code_addr;
extern uint64_t gdt_desc_data_addr;
