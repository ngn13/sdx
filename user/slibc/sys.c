#include "sys.h"

int32_t exit(int32_t code) {
  syscall(0, code);
}

int32_t fork() {
  syscall(1);
}

int32_t exec(char *path, char *argv[], char *envp[]) {
  syscall(2, path, argv, envp);
}
