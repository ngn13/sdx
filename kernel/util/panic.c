#include "util/panic.h"
#include "util/printk.h"
#include "util/asm.h"

#include "core/im.h"
#include "video.h"

void core_dump(task_regs_t *r) {
  // dump all the main registers
  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_LIGHT_RED);
  printf("==== Dumping registers ====\n");

  video_fg_set(VIDEO_COLOR_WHITE);

  printf("| rax = %p | rbx = %p | rcx = %p |\n", r->rax, r->rbx, r->rcx);
  printf("| rdx = %p | r8  = %p | r9  = %p |\n", r->rdx, r->r8, r->r9);
  printf("| r10 = %p | r11 = %p | r12 = %p |\n", r->r10, r->r11, r->r12);
  printf("| r13 = %p | r14 = %p | r15 = %p |\n", r->r13, r->r14, r->r15);
  printf("| rsi = %p | rdi = %p | rbp = %p |\n", r->rsi, r->rdi, r->rbp);

  if (r != (void *)&_panic_local_regs_start)
    printf("| rip = %p | ss  = %p | cs  = %p |\n", r->rip, r->ss, r->cs);

  printf("| cr0 = %p | cr2 = %p | cr3 = %p |\n", _get_cr0(), _get_cr2(), _get_cr3());
  printf("| cr4 = %p | rflags = %p                       |\n", _get_cr4(), r->rflags);

  // dump the stack
  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_LIGHT_RED);
  printf("====== Dumping stack ======");

  video_fg_set(VIDEO_COLOR_WHITE);

  for (uint8_t i = 0; i < 12; i++) {
    if (i % 2 == 0)
      printf("\n%p: ", r->rsp + (i * 8));
    printf("%p ", *(uint64_t *)(r->rsp + (i * 8)));
  }

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_LIGHT_RED);
  printf("\n===========================\n");
}

void __panic_with(task_regs_t *regs, bool do_core_dump, const char *func, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  video_fg_set(VIDEO_COLOR_WHITE);
  video_bg_set(VIDEO_COLOR_RED);
  printf("PANIC");

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_WHITE);
  printf(" at %s: ", func);

  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");

  if (do_core_dump)
    core_dump(regs);

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_LIGHT_RED);

  printf("Kernel crashed, there is no way to recover, you should reboot\n");

  // disable CPU interrupts
  im_disable();

  // hang forever
  _hang();

  // should never return
}
