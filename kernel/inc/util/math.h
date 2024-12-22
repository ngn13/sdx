#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

uint64_t abs(int64_t val);
uint64_t pow(uint64_t n, uint64_t e);

uint64_t gd(int64_t val);
uint64_t gdu(uint64_t val);

uint64_t div_ceil(uint64_t x, uint64_t y);
uint64_t div_floor(uint64_t x, uint64_t y);

#endif
