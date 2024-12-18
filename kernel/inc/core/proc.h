#pragma once
#include "core/sched.h"
#include "fs/fmt.h"

#include "types.h"

typedef int32_t pid_t;

typedef struct proc {
  pid_t        pid;
  pid_t        ppid;
  fmt_info_t   fmt;
  task_t      *task;
  struct proc *next;
} proc_t;

int32_t proc_exec(char *path, char *argv[], char *envp[]); // create a new process
int32_t proc_kill(pid_t pid);                              // murder a process (haram)
proc_t *proc_find(pid_t pid);                              // find a process by its process ID
proc_t *proc_next(proc_t *proc);                           // get the next process the process list
proc_t *proc_current(); // get the current process (NULL if we are not running as a process)
void    proc_init();
