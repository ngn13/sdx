#pragma once
#include "types.h"

/*

 * interrupt manager (IM) functions
 * see: core/im

*/

typedef enum {
  // https://wiki.osdev.org/Exceptions
  IM_INT_DIV_ERR = 0x0,
  // ...
  IM_INT_INV_OPCODE = 0x6,
  // ...
  IM_INT_DOUBLE_FAULT = 0x8,
  // ...
  IM_INT_STACK_SEGMENT_FAIL       = 0xC,
  IM_INT_GENERAL_PROTECTION_FAULT = 0xD,
  IM_INT_PAGE_FAULT               = 0xE,
  // ...
} im_interrupts_t;

typedef struct {
  // all da registers
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

  // interrupt stuff
  uint64_t vector;
  uint64_t error;

  // iret frame
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
} __attribute__((packed)) im_stack_t;

extern void __im_handle();
extern void __im_load_idtr();

// these wrapper handlers are only used for calculating other wrappers addreses
extern void __im_handle_0();
extern void __im_handle_1();

typedef void im_handler_func_t(im_stack_t *s);

void im_init();                     // initialize IDT with the default handler
#define im_enable()  __asm__("sti") // enable the interrupts (set interrupt)
#define im_disable() __asm__("cli") // disable the interrupts (clear interrupt)

void im_set_entry(uint8_t vector, uint8_t dpl); // modfiy a IDT entry

void im_del_handler(uint8_t vector, im_handler_func_t handler); // switch a given IDT entry with the default handler
void im_add_handler(uint8_t vector, im_handler_func_t handler); // set a given IDT entry to a handler
