#pragma once
#include "types.h"

#ifndef __ASSEMBLY__
#include "sched/sched.h"

#define sys_debg(f, ...) pdebg("Sys: (%d:%s) " f, current->pid, __func__, ##__VA_ARGS__)
#define sys_info(f, ...) pinfo("Sys: (%d:%s) " f, current->pid, __func__, ##__VA_ARGS__)
#define sys_fail(f, ...) pfail("Sys: (%d:%s) " f, current->pid, __func__, ##__VA_ARGS__)

struct syscall {
  uint64_t code;
  void    *func;
};

extern struct syscall syscalls[]; // a list of user calls

int32_t sys_handler(); // syscall handler function
int32_t sys_setup();   // setup user syscalls

// system call handlers
void    sys_exit(int32_t code);
pid_t   sys_fork();
int32_t sys_exec(char *path, char *argv[], char *envp[]);
pid_t   sys_wait(int32_t *status);
int32_t sys_open(char *path, int32_t flags, mode_t mode);
int32_t sys_close(int32_t fd);
int64_t sys_read(int32_t fd, void *buf, uint64_t size);
int64_t sys_write(int32_t fd, void *buf, uint64_t size);
int32_t sys_mount(char *source, char *target, char *filesystem, int32_t flags);
int32_t sys_umount(char *target);

#endif
