#include "sys.h"

int32_t exit(int32_t code) {
  __asm__("mov $0, %%rax\n"
          "mov %0, %%rdi\n"
          "syscall" ::"m"(code)
          : "rax", "rdi");
}

int32_t open(const char *path) {
  __asm__("mov $1, %%rax\n"
          "mov %0, %%rdi\n"
          "syscall" ::"m"(path)
          : "rax", "rdi");
}
