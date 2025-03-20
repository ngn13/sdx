#include "syscall.h"
#include "sched/sched.h"

void sys_exit(int32_t code) {
  sys_debg("exiting with code: %d", code);
  sched_exit(code);
  sched(); // will never return
}
