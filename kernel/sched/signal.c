#include "sched/sched.h"
#include "sched/task.h"

#include "mm/heap.h"

#include "util/panic.h"
#include "util/list.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

#define SIG_EXIT_CODE              128
#define __sigset_free(ss)          heap_free(ss)
#define __signal_can_ignore(sig)   (sig != SIGKILL)
#define __signal_call_default(sig) sigdfl[sig - 1](sig)

task_sighand_t sigdfl[SIG_MAX];

void __sighand_term(int32_t sig) {
  task_current->term_code = sig;
  sched_exit(SIG_EXIT_CODE + sig);
}

void __sighand_dump(int32_t sig) {
  core_dump(&task_current->regs);
  task_current->term_code = sig;
  sched_exit(SIG_EXIT_CODE + sig);
}

int32_t task_signal_setup() {
  sigdfl[SIGHUP - 1]  = __sighand_term;
  sigdfl[SIGINT - 1]  = __sighand_term;
  sigdfl[SIGILL - 1]  = __sighand_dump;
  sigdfl[SIGKILL - 1] = __sighand_term;
  sigdfl[SIGSEGV - 1] = __sighand_dump;
  return 0;
}

int32_t task_signal_set(task_t *task, int32_t sig, task_sighand_t hand) {
  if (NULL == task || sig > SIG_MAX || sig < 0)
    return -EINVAL;

  if (SIG_IGN == hand && !__signal_can_ignore(sig))
    return 0;

  task->sighand[sig] = hand;
  return 0;
}

int32_t task_signal_add(task_t *task, int32_t sig) {
  if (NULL == task || sig > SIG_MAX || sig < SIG_MIN)
    return -EINVAL;

  // allocate & setup the signal
  task_sigset_t *signal = heap_alloc(sizeof(task_sigset_t));
  bzero(signal, sizeof(task_sigset_t));
  signal->value = sig;

  // add signal to the end of the queue
  slist_add_end(&task->signal, signal, task_sigset_t);

  return 0;
}

int32_t task_signal_pop(task_t *task) {
  task_sigset_t *cur     = NULL;
  uint64_t       handler = 0;
  int32_t        signal  = -1;

  if (NULL == task)
    return -EINVAL;

  if (NULL == (cur = task->signal))
    return 0;

  signal       = cur->value;
  task->signal = cur->next;
  __sigset_free(cur);

  if (signal > SIG_MAX || signal < SIG_MIN)
    return -EINVAL;

  handler = (uint64_t)task->sighand[signal];

  switch (handler) {
  case (uint64_t)SIG_DFL:
    // call the default handler
    __signal_call_default(signal);
    break;

  case (uint64_t)SIG_IGN:
    // ignore
    break;

  default:
    // TODO: call the actual task handler
    break;
  }

  return signal;
}
