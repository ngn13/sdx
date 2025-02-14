#ifndef __ASSEMBLY__
#include "types.h"

extern uint64_t _start_addr;
extern uint64_t _end_addr;

uint64_t _get_cr0();
uint64_t _get_cr2();
uint64_t _get_cr3();
uint64_t _get_cr4();

void     _msr_write(uint32_t msr, uint64_t val);
uint64_t _msr_read(uint32_t msr);

void _hang();

#endif
