#include "stdint.h"

#define __syscall_1(sys) __asm__("syscall" ::"ra"(sys) : "rdi");
#define __syscall_2(sys, arg1)                                                                                         \
  __asm__("mov %1, %%rdi\n"                                                                                            \
          "syscall" ::"ra"(sys),                                                                                       \
      "m"(arg1)                                                                                                        \
      : "rdi");
#define __syscall_3(sys, arg1, arg2)                                                                                   \
  __asm__("mov %1, %%rdi\n"                                                                                            \
          "mov %2, %%rsi\n"                                                                                            \
          "syscall" ::"ra"(sys),                                                                                       \
      "m"(arg1),                                                                                                       \
      "m"(arg2)                                                                                                        \
      : "rdi");
#define __syscall_4(sys, arg1, arg2, arg3)                                                                             \
  __asm__("mov %1, %%rdi\n"                                                                                            \
          "mov %2, %%rsi\n"                                                                                            \
          "mov %3, %%rdx\n"                                                                                            \
          "syscall" ::"ra"(sys),                                                                                       \
      "m"(arg1),                                                                                                       \
      "m"(arg2),                                                                                                       \
      "m"(arg3)                                                                                                        \
      : "rdi");

#define __syscall_get(_1, _2, _3, _4, name, ...) name
#define syscall(...)                             __syscall_get(__VA_ARGS__, __syscall_4, __syscall_3, __syscall_2, __syscall_1)(__VA_ARGS__)

int32_t exit(int32_t code);
int32_t fork();
int32_t exec(char *path, char *argv[], char *envp[]);
