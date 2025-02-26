#include "types.h"

// syscall function (see sys.S)
extern uint64_t syscall(uint64_t num, ...);

// syscalls (see sys.c)
_Noreturn void exit(int32_t code);
pid_t          fork();
int32_t        exec(char *path, char *argv[], char *envp[]);
pid_t          wait(int32_t *status);
