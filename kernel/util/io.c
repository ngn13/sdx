#include "util/io.h"

bool out8(uint16_t port, uint8_t val) {
  if (port == 0)
    return false;

  __asm__("mov %0, %%dx\n"
          "mov %1, %%al\n"
          "out %%al, %%dx\n"
          :
          : "ir"(port), "ir"(val)
          : "rax", "rdx");

  return true;
}

bool out16(uint16_t port, uint16_t val) {
  if (port == 0)
    return false;

  __asm__("mov %0, %%dx\n"
          "mov %1, %%ax\n"
          "out %%ax, %%dx\n"
          :
          : "ir"(port), "ir"(val)
          : "rax", "rdx");

  return true;
}

bool out32(uint16_t port, uint32_t val) {
  if (port == 0)
    return false;

  __asm__("mov %0, %%dx\n"
          "mov %1, %%eax\n"
          "out %%eax, %%dx\n"
          :
          : "ir"(port), "ir"(val)
          : "rax", "rdx");

  return true;
}

uint8_t in8(uint16_t port) {
  if (port == 0)
    return 0;

  uint8_t val = 0;

  __asm__("xor %%rax, %%rax\n"
          "mov %1, %%dx\n"
          "in %%dx, %%al\n"
          "mov %%al, %0\n"
          : "=r"(val)
          : "ir"(port)
          : "rax", "rdx");

  return val;
}

uint16_t in16(uint16_t port) {
  if (port == 0)
    return 0;

  uint16_t val = 0;

  __asm__("xor %%rax, %%rax\n"
          "mov %1, %%dx\n"
          "in %%dx, %%ax\n"
          "mov %%ax, %0\n"
          : "=r"(val)
          : "ir"(port)
          : "rax", "rdx");

  return val;
}

uint32_t in32(uint16_t port) {
  if (port == 0)
    return 0;

  uint32_t val = 0;

  __asm__("xor %%rax, %%rax\n"
          "mov %1, %%dx\n"
          "in %%dx, %%eax\n"
          "mov %%eax, %0\n"
          : "=r"(val)
          : "ir"(port)
          : "rax", "rdx");

  return val;
}

// used to wait for few microseconds by writing to an unused port
void io_wait() {
  out8(0x80, 0);
}

bool out8_wait(uint16_t port, uint8_t val) {
  bool ret = out8(port, val);
  io_wait();
  return ret;
}

bool out16_wait(uint16_t port, uint16_t val) {
  bool ret = out16(port, val);
  io_wait();
  return ret;
}

bool out32_wait(uint16_t port, uint32_t val) {
  bool ret = out32(port, val);
  io_wait();
  return ret;
}
