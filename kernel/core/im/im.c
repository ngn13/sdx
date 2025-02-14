#include "boot/boot.h"
#include "core/im.h"

#include "mm/vmm.h"
#include "mm/heap.h"

#include "util/printk.h"
#include "util/panic.h"
#include "util/list.h"
#include "util/mem.h"

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
  im_handler_func_t       *func;       // handler function
  im_handler_prio_t        prio;       // handler function priority
  uint8_t                  vector;     // selected vector for the handler
  bool                     is_enabled; // is the handler enabled
  struct im_handler_entry *next, *pre; // next and previous handler
};

struct im_handler {
  uint64_t                 count;
  struct im_handler_entry *head, *tail;
};

struct tss {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t reserved2;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t reserved3;
  uint16_t reserved4;
  uint16_t io_bitmap_offset;
} __attribute__((packed));

#define IM_IDT_SIZE        sizeof(im_idt)
#define IM_IDT_ENTRY_SIZE  sizeof(struct im_desc)
#define IM_IDT_ENTRY_COUNT (IM_IDT_SIZE / IM_IDT_ENTRY_SIZE)

struct tss        im_tss;
struct im_desc    im_idt[256];
struct im_idtr    im_idtr = {.size = IM_IDT_SIZE - 1, .addr = (uint64_t)im_idt};
struct im_handler im_handler;

/*

 * default interrupt handler, this is called by __im_handle (see core/im.S)
 * we can't directly call this right through the IDT entry, as we need to
 * make sure that the stack and everything is setup correctly when we iret

*/
void im_handle(im_stack_t *stack) {
  for (uint8_t i = IM_HANDLER_PRIO_FIRST; i <= IM_HANDLER_PRIO_LAST; i++)
    dlist_reveach(&im_handler.tail, struct im_handler_entry) if (cur->prio == i && cur->vector == stack->vector)
        cur->func(stack);
}

void im_set_entry(uint8_t vector, uint8_t dpl) {
  struct im_desc *d = &im_idt[vector];

  // GDT code segment offset for the CS
  d->selector = gdt_offset(gdt_desc_kernel_code_addr);

  /*

   * gate type = 0b1110/0xE (interrupt gate)
   * [random fucking zero here]
   * DPL = dpl
   * P = 1 (it needs to be set as 1 otherwise its not valid)

  */
  d->attr = (1 << 7) | ((dpl & 0b11) << 5) | 0b1110;

  // for now we are just gonna disable the IST (and clear out the reserved area as well)
  d->ist = 0;
}

// add/set interrupt in the IDT
void im_add_handler(uint8_t vector, im_handler_prio_t prio, im_handler_func_t handler) {
  if (NULL == handler)
    return;

  // check if the handler is already in the list
  dlist_foreach(&im_handler.head, struct im_handler_entry) {
    if (cur->vector == vector && cur->func == handler)
      return;
  }

  // create a new entry
  struct im_handler_entry *entry = heap_alloc(sizeof(struct im_handler_entry));
  entry->next                    = NULL;
  entry->vector                  = vector;
  entry->func                    = handler;
  entry->prio                    = prio;

  // add the new entry to the list
  dlist_add(&im_handler.head, &im_handler.tail, entry);

  // increment the handler count
  im_handler.count++;
}

void im_del_handler(uint8_t vector, im_handler_func_t handler) {
  if (NULL == handler)
    return;

  // if list is empty it can't contain the handler
  if (NULL == im_handler.head || im_handler.count == 0)
    return;

  // check if the list contains the handler
  struct im_handler_entry *entry = NULL;

  dlist_foreach(&im_handler.head, struct im_handler_entry) {
    if (cur->vector == vector && cur->func == handler) {
      entry = cur;
      break;
    }
  }

  // the entry not found
  if (NULL == entry)
    return;

  // remove the handler from the list
  dlist_del(&im_handler.head, &im_handler.tail, entry, struct im_handler_entry);
  im_handler.count--;

  heap_free(entry);
}

void im_disable_handler(uint8_t vector, im_handler_func_t handler) {
  dlist_foreach(&im_handler.head, struct im_handler_entry) {
    if (cur->vector == vector && cur->func == handler) {
      cur->is_enabled = false;
      break;
    }
  }
}

void im_enable_handler(uint8_t vector, im_handler_func_t handler) {
  dlist_foreach(&im_handler.head, struct im_handler_entry) {
    if (cur->vector == vector && cur->func == handler) {
      cur->is_enabled = true;
      break;
    }
  }
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
    d->address_low  = (wrapper_addr + (wrapper_size * i)) & UINT16_MAX;
    d->address_mid  = ((wrapper_addr + (wrapper_size * i)) >> 16) & UINT16_MAX;
    d->address_high = ((wrapper_addr + (wrapper_size * i)) >> 32) & UINT32_MAX;

    // set the default entry flags
    im_set_entry(i, 0);
  }

  // init the handler list
  bzero(&im_handler, sizeof(im_handler));

  // setup the TSS
  bzero(&im_tss, sizeof(struct tss));

  if ((im_tss.rsp0 = (uint64_t)vmm_map(1, VMM_FLAGS_DEFAULT)) == NULL)
    panic("Failed to allocate memory for the TSS");

  im_tss.rsp0 += VMM_PAGE_SIZE;
  pdebg("IM: Created TSS stack at 0x%x", im_tss.rsp0);

  gdt_tss_set(&im_tss, (sizeof(struct tss) - 1));

  // load the IDTR
  __asm__("lidt (%0)" ::"rm"(&im_idtr));

  // load the TSS
  __asm__("ltr %0" ::"r"(gdt_offset(gdt_desc_tss_addr)));
}
