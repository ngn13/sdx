#pragma once

#include "core/pic.h"
#include "core/im.h"

#include "limits.h"
#include "limits.h"
#include "types.h"

/*

 * scheduler defitinions
 * see core/sched.c for the implementation

*/

typedef struct {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rsp;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t rip;
  uint64_t rflags;
  uint64_t cs;
  uint64_t ss;
} task_regs_t;

enum {
  TASK_STATE_READY  = 0,
  TASK_STATE_ACTIVE = 1,
  TASK_STATE_DEAD   = 2,
};

typedef struct task {
  char        name[NAME_MAX + 1];
  int32_t     id;    // task ID
  task_regs_t regs;  // saved task registers
  uint8_t     ticks; // ticks (runtime) for this task
#define TASK_TICKS_DEFAULT (50)
  uint8_t state : 2; // state of this task (see the enum above)
  uint8_t prio  : 6; // task priority
#define TASK_PRIO_MAX     (63)
#define TASK_PRIO_MIN     (0)
#define TASK_PRIO_DEFAULT (20)
  uint8_t ring : 2;
#define TASK_RING_KERNEL (0)
#define TASK_RING_USER   (3)
  struct task *next; // next task in the linked list
} task_t;

#define TASK_STACK_SIZE (16) // 16 pages, 64KB

extern task_t *current;

#define sched() __asm__("int %0" ::"i"(pic_to_int(PIC_IRQ_TIMER)))
int32_t sched_new(const char *name, void *func, uint8_t ring);
task_t *sched_find(int32_t id);
#define sched_kill()                                                                                                   \
  do {                                                                                                                 \
    current->state = TASK_STATE_DEAD;                                                                                  \
    sched();                                                                                                           \
  } while (0)
int32_t sched_init();

#define sched_set_prio(p)                                                                                              \
  do {                                                                                                                 \
    if (NULL != current)                                                                                               \
      current->prio = p;                                                                                               \
  } while (0)

#define sched_set_ticks(t)                                                                                             \
  do {                                                                                                                 \
    if (NULL != current)                                                                                               \
      current->ticks = t;                                                                                              \
  } while (0)

enum {
  TASK_LEVEL_CRITIKAL = 0,
  TASK_LEVEL_HIGH     = 1,
  TASK_LEVEL_MEDIUM   = 2,
  TASK_LEVEL_LOW      = 3,
};

#define sched_level(l)                                                                                                 \
  switch (l) {                                                                                                         \
  case TASK_LEVEL_CRITIKAL:                                                                                            \
    sched_set_prio(TASK_PRIO_MAX);                                                                                     \
    sched_set_ticks(UINT8_MAX);                                                                                        \
    break;                                                                                                             \
  case TASK_LEVEL_HIGH:                                                                                                \
    sched_set_prio(TASK_PRIO_MAX / 2);                                                                                 \
    sched_set_ticks(100);                                                                                              \
    break;                                                                                                             \
  case TASK_LEVEL_MEDIUM:                                                                                              \
    sched_set_prio(TASK_PRIO_DEFAULT);                                                                                 \
    sched_set_ticks(TASK_TICKS_DEFAULT);                                                                               \
    break;                                                                                                             \
  case TASK_LEVEL_LOW:                                                                                                 \
    sched_set_prio(0);                                                                                                 \
    sched_set_ticks(10);                                                                                               \
    break;                                                                                                             \
  }
