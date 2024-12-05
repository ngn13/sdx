#pragma once
#include "types.h"

/*

 * interrupt manager (IM) functions
 * see: core/im

*/

typedef enum im_interrupts {
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

extern void __im_handle();
extern void __im_load_idtr();

// these wrapper handlers are only used for calculating other wrappers addreses
extern void __im_handle_0();
extern void __im_handle_1();

typedef void im_handler_func(uint8_t v);

void im_init();                     // initialize IDT with the default handler
#define im_enable()  __asm__("sti") // enable the interrupts (set interrupt)
#define im_disable() __asm__("cli") // disable the interrupts (clear interrupt)

void im_set_entry(uint8_t vector, uint8_t dpl); // modfiy a IDT entry

void im_del_handler(uint8_t vector, im_handler_func handler); // switch a given IDT entry with the default handler
void im_add_handler(uint8_t vector, im_handler_func handler); // set a given IDT entry to a handler
