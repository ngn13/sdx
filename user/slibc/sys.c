#include "sys.h"

void exit(int32_t code) {
  syscall(0, code);

  // for some reason if the syscall fails, just hang
  while (true)
    __asm__("hlt");
}

pid_t fork() {
  return syscall(1);
}

int32_t exec(char *path, char *argv[], char *envp[]) {
  return syscall(2, path, argv, envp);
}

pid_t wait(int32_t *status) {
  return syscall(3, status);
}

int32_t open(char *path, int32_t flags, mode_t mode) {
  return syscall(4, path, flags, mode);
}

int32_t close(int32_t fd) {
  return syscall(5, fd);
}

int64_t read(int32_t fd, void *buf, uint64_t size) {
  return syscall(6, fd, buf, size);
}

int64_t write(int32_t fd, void *buf, uint64_t size) {
  return syscall(7, fd, buf, size);
}
int32_t mount(char *source, char *target, char *filesystem, int32_t flags) {
  return syscall(8, source, target, filesystem, flags);
}

int32_t umount(char *target) {
  return syscall(9, target);
}
