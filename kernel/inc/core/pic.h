#pragma once
#include "types.h"

/*

 * programmable interrupt controller (PIC) functions
 * see: core/pic.c

*/
typedef enum pic_irq {
  // https://wiki.osdev.org/Interrupts#Standard_ISA_IRQs
  PIC_IRQ_TIMER    = 0,
  PIC_IRQ_KEYBOARD = 1,
  PIC_IRQ_COM2     = 3,
  PIC_IRQ_COM1     = 4,
  PIC_IRQ_LPT2     = 5,
  PIC_IRQ_FLOPPY   = 6,
  PIC_IRQ_LPT1     = 7,
  PIC_IRQ_CMOS     = 8,
  PIC_IRQ_MOUSE    = 12,
  PIC_IRQ_ATA1     = 14,
  PIC_IRQ_ATA2     = 15,
} pic_irq_t;

// IDT vector offset, 32 is enough to make sure we don't use exception interrupt vectors
#define PIC_VECTOR_OFFSET 33

bool pic_init(); // initialize the PIC

bool pic_enable();  // enable all PIC interrupts
bool pic_disable(); // disable all PIC interrupts

bool pic_mask(pic_irq_t i);   // mask a PIC interrupt (offset included)
bool pic_unmask(pic_irq_t i); // unmask a PIC interrupt (offset included)
