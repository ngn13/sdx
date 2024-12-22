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
int32_t user_exit(int32_t code);
int32_t user_fork();
int32_t user_exec(char *path, char *argv[], char *envp[]);

#endif
