#pragma once

#include "types.h"

#define BOOT_KERNEL_ADDR 0xfffffff800000000
#define BOOT_STACK_SIZE  0x10000

#ifndef __ASSEMBLY__

extern uint32_t boot_end_addr;

#endif
