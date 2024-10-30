#include "util/io.h"

bool out8(uint16_t port, uint8_t val) {
  if (port == 0)
    return false;

  __asm__(".intel_syntax noprefix;"
          "mov dx, %0;"
          "mov al, %1;"
          "out dx, al;"
          ".att_syntax;"
          :
          : "ir"(port), "ir"(val)
          : "rax", "rdx");

  return true;
}

bool out16(uint16_t port, uint16_t val) {
  if (port == 0)
    return false;

  __asm__(".intel_syntax noprefix;"
          "mov dx, %0;"
          "mov ax, %1;"
          "out dx, ax;"
          ".att_syntax;"
          :
          : "ir"(port), "ir"(val)
          : "rax", "rdx");

  return true;
}

bool out32(uint16_t port, uint32_t val) {
  if (port == 0)
    return false;

  __asm__(".intel_syntax noprefix;"
          "mov dx, %0;"
          "mov eax, %1;"
          "out dx, eax;"
          ".att_syntax;"
          :
          : "ir"(port), "ir"(val)
          : "rax", "rdx");

  return true;
}

uint8_t in8(uint16_t port) {
  if (port == 0)
    return 0;

  uint8_t val = 0;

  __asm__(".intel_syntax noprefix;"
          "xor rax, rax;"
          "mov dx, %1;"
          "in al, dx;"
          "mov %0, al;"
          ".att_syntax;"
          : "=r"(val)
          : "ir"(port)
          : "rax", "rdx");

  return val;
}

uint16_t in16(uint16_t port) {
  if (port == 0)
    return 0;

  uint16_t val = 0;

  __asm__(".intel_syntax noprefix;"
          "xor rax, rax;"
          "mov dx, %1;"
          "in ax, dx;"
          "mov %0, ax;"
          ".att_syntax;"
          : "=r"(val)
          : "ir"(port)
          : "rax", "rdx");

  return val;
}

uint32_t in32(uint16_t port) {
  if (port == 0)
    return 0;

  uint32_t val = 0;

  __asm__(".intel_syntax noprefix;"
          "xor rax, rax;"
          "mov dx, %1;"
          "in eax, dx;"
          "mov %0, eax;"
          ".att_syntax;"
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
