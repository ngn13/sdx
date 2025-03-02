#include "types.h"

// syscall function (see sys.S)
extern uint64_t syscall(uint64_t num, ...);

// syscalls (see sys.c)
_Noreturn void exit(int32_t code);
pid_t          fork();
int32_t        exec(char *path, char *argv[], char *envp[]);
pid_t          wait(int32_t *status);
int32_t        open(char *path, int32_t flags, mode_t mode);
int32_t        close(int32_t fd);
int64_t        read(int32_t fd, void *buf, uint64_t size);
int64_t        write(int32_t fd, void *buf, uint64_t size);
