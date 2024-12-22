#pragma once
#include "sched/task.h"
#include "types.h"

/*

 * util/panic.S, local register storage, stores local registers
 * if task_regs_t is NULL

*/
extern uint64_t _panic_local_regs_start;
extern uint64_t _panic_local_regs_end;

void _panic_with(task_regs_t *regs, const char *func, char *fmt, ...);
void core_dump(task_regs_t *regs);

#define panic(f, ...)         _panic_with(NULL, __func__, f, ##__VA_ARGS__)
#define panic_with(r, f, ...) _panic_with(r, __func__, f, ##__VA_ARGS__)
