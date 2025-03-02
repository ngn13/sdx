#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

struct user_call {
  uint64_t code;
  void    *func;
};

extern struct user_call user_calls[]; // a list of user calls

int32_t _user_handler(); // syscall handler function
int32_t user_setup();    // setup user syscalls

// system call handlers
void    user_exit(int32_t code);
pid_t   user_fork();
int32_t user_exec(char *path, char *argv[], char *envp[]);
pid_t   user_wait(int32_t *status);
int32_t user_open(char *path, int32_t flags, mode_t mode);
int32_t user_close(int32_t fd);

#endif
