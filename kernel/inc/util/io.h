#pragma once
#include "types.h"

bool out8(uint16_t port, uint8_t val);
bool out16(uint16_t port, uint16_t val);
bool out32(uint16_t port, uint32_t val);

uint8_t  in8(uint16_t port);
uint16_t in16(uint16_t port);
uint32_t in32(uint16_t port);

void io_wait();

bool out8_wait(uint16_t port, uint8_t val);
bool out16_wait(uint16_t port, uint16_t val);
bool out32_wait(uint16_t port, uint32_t val);
