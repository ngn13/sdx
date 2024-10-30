#include "types.h"

#include "boot/gdt.h"
#include "core/im.h"
#include "mm/vmm.h"

#include "util/io.h"
#include "util/mem.h"
#include "util/panic.h"
#include "util/printk.h"

/*

 * interrupt manager (IM)

 * interrupts tell CPU to stop whatever it's doing and do something else
 * this "something else" is described to the CPU using the interrupt descriptor table (IDT)

 * IDT stores an entry (descriptor) for each interrupt, which tells CPU how to handle
 * the interrupt, the handlers we provide with IM, will make sure the CPU handles the interrupt
 * correctly, and it safely goes back to what it was doing

 * there are also different types of interrupts: exceptions and IRQs
 * exceptions happen when something goes wrong, however the IRQs are raised by the PIC/APIC
 * and they are used to communicate with the hardware (see core/pic.c for more info)

 * the core/im.S is also contains related assembly code as it's not really possible to handle
 * interrupts with C

*/

// https://wiki.osdev.org/Interrupt_Descriptor_Table#Example_Code_2
struct im_desc {
  uint16_t address_low;  // handler address bits (lower)
  uint16_t selector;     // GDT CS selector
  uint8_t  ist;          // first 2 bits for interrupt stack table offset, rest is reserved (zero)
  uint8_t  attr;         // gate type (3), zero (1), DPL (CPU privilege level) (2), P (Present) (1)
  uint16_t address_mid;  // handler address bits (middle)
  uint32_t address_high; // handler address bits (higher)
  uint32_t zero;         // reserved (zero)
} __attribute__((packed));

struct im_idtr {
  uint16_t size;
  uint64_t addr;
} __attribute__((packed));

struct im_handler_entry {
  struct im_handler_entry *next;   // next handler
  im_handler_func         *func;   // handler function
  uint8_t                  vector; // selected vector for the handler
};

struct im_handler {
  uint64_t                 count;
  struct im_handler_entry *head;
  struct im_handler_entry *tail;
};

#define IM_IDT_SIZE        sizeof(im_idt)
#define IM_IDT_ENTRY_SIZE  sizeof(struct im_desc)
#define IM_IDT_ENTRY_COUNT (IM_IDT_SIZE / IM_IDT_ENTRY_SIZE)

struct im_desc    im_idt[256];
struct im_idtr    im_idtr = {.size = IM_IDT_SIZE - 1, .addr = (uint64_t)im_idt};
struct im_handler im_handler;

struct im_saved_state {
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
} __attribute__((packed));

/*

 * default interrupt handler, this is called by __im_handle (see core/im.S)
 * we can't directly call this right through the IDT entry, as we need to
 * make sure that the stack and everything is setup correctly when we iret

*/
void im_handle(uint8_t vector, struct im_saved_state *state) {
  struct im_handler_entry *entry = im_handler.head;

  while (NULL != entry) {
    if (entry->vector == vector)
      entry->func(vector);
    entry = entry->next;
  }

  /*

   * panic on critical faults/traps
   * list can be found at https://wiki.osdev.org/Exceptions

  */
  switch (vector) {
  case IM_INT_DIV_ERR:
    panic(__func__, "Received an interrupt for division by zero");
    break;

  case IM_INT_INV_OPCODE:
    panic(__func__, "Received an interrupt for bad opcode");
    break;

  case IM_INT_DOUBLE_FAULT:
    panic(__func__, "Received an interrupt for double fault");
    break;

  case IM_INT_GENERAL_PROTECTION_FAULT:
    panic(__func__, "Received an interrupt for GPF");
    break;

  case IM_INT_PAGE_FAULT:
    panic(__func__, "Received an interrupt for PF");
    break;
  }

  printk(KERN_DEBG, "IM: handled interrupt %d\n", vector);
}

void im_set_entry(uint8_t vector, uint8_t dpl) {
  struct im_desc *d = &im_idt[vector];

  // GDT code segment offset for the CS
  d->selector = gdt_desc_code_addr - gdt_start_addr;

  /*

   * gate type = 0b1110/0xE (interrupt gate)
   * [random fucking zero here]
   * dpl = 0 (ring 0)
   * p = 1 (it needs to be set as 1 otherwise its not valid)

  */
  d->attr = (0b1 << 7) | ((dpl & 0b11) << 5) | 0b1110;

  // for now we are just gonna disable the IST (and clear out the reserved area as well)
  d->ist = 0;
}

// add/set interrupt in the IDT
void im_add_handler(uint8_t vector, im_handler_func handler) {
  if (NULL == handler)
    return;

  struct im_handler_entry *entry = im_handler.head;

  // check if the handler is already in the list
  while (NULL != entry) {
    if (entry->vector == vector && entry->func == handler)
      return;
    entry = entry->next;
  }

  // create a new entry
  entry         = vmm_alloc(sizeof(struct im_handler_entry));
  entry->next   = NULL;
  entry->vector = vector;
  entry->func   = handler;

  // check if this is the first entry (head)
  if (NULL == im_handler.head)
    im_handler.head = entry;

  // the new entry is the tail
  im_handler.tail->next = entry;
  im_handler.tail       = entry;

  // increment the handler count
  im_handler.count++;
}

void im_del_handler(uint8_t vector, im_handler_func handler) {
  if (NULL == handler)
    return;

  // if list is empty it can't contain the handler
  if (NULL == im_handler.head || NULL == im_handler.tail || im_handler.count == 0)
    return;

  // check if the list contains the handler
  struct im_handler_entry *entry = im_handler.head, *pre = NULL;

  while (NULL != entry) {
    if (entry->vector == vector && entry->func == handler)
      break;

    pre   = entry;
    entry = entry->next;
  }

  // the entry not found
  if (NULL == entry)
    return;

  if (NULL == pre)
    im_handler.head = NULL;
  else
    pre->next = entry;

  if (entry == im_handler.head)
    im_handler.head = entry->next;

  if (entry == im_handler.tail) {
    if (NULL != entry->next)
      im_handler.tail = entry->next;
    else
      im_handler.tail = pre;
  }

  vmm_free(entry);
}

// initialize the interrupt manager
void im_init() {
  struct im_desc    *d = NULL;
  struct im_handler *h = NULL;

  uint64_t wrapper_addr = (uint64_t)__im_handle_0;
  uint64_t wrapper_size = __im_handle_1 - __im_handle_0;

  for (uint16_t i = 0; i < IM_IDT_ENTRY_COUNT; i++) {
    d = &im_idt[i];

    // setup all the address bits
    d->address_low  = (wrapper_addr + (wrapper_size * i)) & 0xffff;
    d->address_mid  = ((wrapper_addr + (wrapper_size * i)) >> 16) & 0xffff;
    d->address_high = ((wrapper_addr + (wrapper_size * i)) >> 32) & 0xffff;

    // set the default entry flags
    im_set_entry(i, 0);
  }

  // init the handler list
  bzero(&im_handler, sizeof(im_handler));

  // load the IDTR (see im/handler.S)
  __im_load_idtr();
}
