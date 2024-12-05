#include "util/panic.h"
#include "util/printk.h"

#include "core/im.h"
#include "video.h"

struct panic_state {
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
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t rip;
  char    *message;
  char    *func_name;
} __attribute__((packed));

void __panic_halt() {
  // disable CPU interrupts
  im_disable();

  // halt forever
  while (true)
    __asm__("hlt");
}

void __panic_dump(struct panic_state *s) {
  /*

   * panic_state comes after the push'ed rbp and the return address
   * so the stack frame of the caller of the panic, ends (or really, starts)
   * right after the panic_state end address + 16

  */
#define panic_frame_addr() (((uint64_t)s) + sizeof(struct panic_state) + 16)

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_LIGHT_RED);
  printf("==== Dumping registers ====\n");
  video_fg_set(VIDEO_COLOR_WHITE);

  printf("rax = %p | r10 = %p | rsi = %p\n", s->rax, s->r10, s->rsi);
  printf("rbx = %p | r11 = %p | rdi = %p\n", s->rbx, s->r11, s->rdi);
  printf("rcx = %p | r12 = %p | rbp = %p\n", s->rcx, s->r12, s->rbp);
  printf("rdx = %p | r13 = %p | rip = %p\n", s->rdx, s->r13, s->rip);
  printf("r8  = %p | r14 = %p\n", s->r8, s->r14);
  printf("r9  = %p | r15 = %p\n", s->r9, s->r15);

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_LIGHT_RED);
  printf("==== Dumping stack ====");
  video_fg_set(VIDEO_COLOR_WHITE);

  for (uint8_t i = 0; i < 12; i++) {
    if (i % 2 == 0)
      printf("\n%p: ", panic_frame_addr() + (i * 8));
    printf("%p ", *(uint64_t *)(panic_frame_addr() + (i * 8)));
  }

  printf("\n");
}

void __panic(struct panic_state *state) {
  video_fg_set(VIDEO_COLOR_WHITE);
  video_bg_set(VIDEO_COLOR_RED);
  printf("PANIC");

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_WHITE);
  printf(" %s: ", state->func_name);

  printf("%s\n", state->message);

  __panic_dump(state);

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_LIGHT_RED);

  printf("Kernel crashed, there is no way to recover, you should reboot\n");

  // should never return
  __panic_halt();
}
