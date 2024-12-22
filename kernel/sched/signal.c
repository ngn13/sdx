#include "sched/signal.h"
#include "sched/sched.h"

#include "mm/vmm.h"

#include "util/mem.h"
#include "util/printk.h"
#include "util/panic.h"
#include "util/list.h"

#include "errno.h"
#include "types.h"

#define SIG_EXIT_CODE 128

sighand_t sigdfl[SIG_MAX];

void sighand_term(int32_t sig) {
  sched_exit(SIG_EXIT_CODE + sig);
}

void sighand_dump(int32_t sig) {
  core_dump(&current->regs);
  sched_exit(SIG_EXIT_CODE + sig);
}

int32_t task_signal_pop(task_t *task) {
  sigset_t *cur    = task->signal;
  int32_t   signal = -1;

  if (NULL != cur) {
    signal = cur->value;

    // more efficient then slist_del
    task->signal = cur->next;
    __sigset_free(cur);
  }

  return signal;
}

void task_signal_call(task_t *task, int32_t sig) {
  if (NULL == task || sig > SIG_MAX || sig < SIG_MIN)
    return;

  uint64_t handler = (uint64_t)task->sighand[sig];

  switch (handler) {
  case (uint64_t)SIG_DFL:
    // call the default handler
    __signal_call_default(sig);
    break;

  case (uint64_t)SIG_IGN:
    // ignore
    break;

  default:
    // TODO: call the actual task handler
    break;
  }
}

int32_t task_signal(task_t *task, int32_t sig, sighand_t hand) {
  if (NULL == task || sig > SIG_MAX || sig < 0)
    return -EINVAL;

  if (SIG_IGN == hand && !__signal_can_ignore(sig))
    return 0;

  task->sighand[sig] = hand;
  return 0;
}

int32_t task_kill(task_t *task, int32_t sig) {
  if (NULL == task || sig > SIG_MAX || sig < SIG_MIN)
    return -EINVAL;

  sigset_t *signal = vmm_alloc(sizeof(sigset_t));
  bzero(signal, sizeof(sigset_t));
  signal->value = sig;

  slist_add(&task->signal, signal, sigset_t);
  return 0;
}
