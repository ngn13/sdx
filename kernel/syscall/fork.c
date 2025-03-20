#include "sched/sched.h"
#include "syscall.h"

pid_t sys_fork() {
  sys_debg("forking the current task");

  /*

   * save the caller PID to check if we are running
   * as the parent or the child after the fork

  */
  pid_t caller = current->pid;

  /*

   * setting our TASK_STATE_FORK will make it so the next
   * time the scheduler is called the current task will be
   * forked, to fork the task right now, we call the
   * scheduler ourselves

  */
  sched_state(TASK_STATE_FORK);
  sched();

  // parent returns child PID
  if (caller == current->pid)
    return task_current->cpid;

  // child returns 0
  return 0;
}
