#ifndef __ASSEMBLY__
#include "types.h"

extern uint64_t _start_addr;
extern uint64_t _end_addr;

uint64_t _get_cr0();
uint64_t _get_cr2();
uint64_t _get_cr3();
uint64_t _get_cr4();

#define MSR_EFER  (0xC0000080) // EFER (Extended Feature Enables)
#define MSR_STAR  (0xC0000081) // STAR (System Call Target Address)
#define MSR_LSTAR (0xC0000082) // LSTAR (IA-32e Mode System Call Target Address)
#define MSR_FMASK (0xC0000084) // FMASK (System Call Flag Mask)

void     _msr_write(uint32_t msr, uint64_t val);
uint64_t _msr_read(uint32_t msr);

void _hang();

#endif
