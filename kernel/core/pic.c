#include "util/io.h"
#include "util/panic.h"
#include "util/printk.h"

#include "core/im.h"
#include "core/pic.h"

/*

 * programmable interrupt controller functions (PIC)

 * to learn more about PICs check out these links:
 * https://wiki.osdev.org/8259_PIC
 * https://en.wikipedia.org/wiki/Intel_8259

 * basically the original 8259 offered a PIC with 8 IRQ ports
 * but it also had this cascade mode where you can plug in other PIC(s) into IRQ ports
 * so you could get 64 IRQs (8 IRQ ports, 8 PICs, 8*8 = 64)

 * the PIC that you plug the other PIC(s) into is called the master
 * and the PIC(s) you plug into the master is called slaves

 * IBM PC AT took advantage of this cascade mode by connecting a single slave to
 * master PIC's third IRQ port, and for some reason this two-chip architecture is
 * still used and available in modern systems, and hasn't changed

 * as the time went on, PICs got embedded into motherboard's southboard
 * also intel dropped APIC which is advanced PIC, and it is basically available on
 * any multiproccessor system, however it's more complicated and shit

 * so i'll go with the basic PIC and who the actual fuck is gonna use all the IRQs anyway
 * i literally only need one which is the keybord - at least for now

*/

// master/slave PIC ports
#define PIC_MASTER_COMM 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_COMM  0xA0
#define PIC_SLAVE_DATA  0xA1

#define PIC_IRQ_MAX 7 // max IRQ number (for a single PIC)
#define PIC_COUNT   2 // total PIC count
#define PIC_IRQ_TOTAL                                                                                                  \
  (((PIC_COUNT - 1) * (PIC_IRQ_MAX + 1)) + ((PIC_IRQ_MAX + 1) - (PIC_COUNT - 1))) // max IRQ count (for all the PICs)

// masks a given interrupt (offset included)
bool pic_mask(pic_irq_t i) {
  // check if the interrupt number is valid
  if (i < 0 || i > PIC_IRQ_TOTAL)
    return false;

  // check if is the interrupt is handled by the slave
  uint16_t port = PIC_MASTER_DATA;

  if (i > PIC_IRQ_MAX)
    port = PIC_SLAVE_DATA;

  // get the currently masked interrupts, and mask the requested one
  if (!out8_wait(port, in8(port) | 1 << i)) {
    printk(KERN_FAIL, "PIC: Failed to mask %d (port %d), I/O failure\n", i, port);
    return false;
  }

  return true;
}

// unmasks a given interrupt (offset included)
bool pic_unmask(pic_irq_t i) {
  // check if the interrupt number is valid
  if (i < 0 || i > PIC_IRQ_TOTAL)
    return false;

  // check if is the interrupt is handled by the slave
  uint16_t port = PIC_MASTER_DATA;

  if (i > PIC_IRQ_MAX)
    port = PIC_SLAVE_DATA;

  // get the currently masked interrupts, and mask the requested one
  if (!out8_wait(port, in8(port) & ~(1 << i))) {
    printk(KERN_FAIL, "PIC: Failed to unmask %d (port %d), I/O failure\n", i, port);
    return false;
  }

  return true;
}

// send end of interrupt signal for a specific IRQ
bool pic_eoi(pic_irq_t i) {
  return i > PIC_IRQ_MAX ? out8_wait(PIC_SLAVE_COMM, 0x20) : out8_wait(PIC_MASTER_COMM, 0x20);
}

// just a helper function to write the same value to the slave and the master
bool __pic_out8_all(bool comm, uint8_t val) {
  return out8_wait(comm ? PIC_MASTER_COMM : PIC_MASTER_DATA, val) &&
         out8_wait(comm ? PIC_SLAVE_COMM : PIC_SLAVE_DATA, val);
}

// default PIC interrupt (IRQ) handler
void __pic_handler_default(im_stack_t *stack) {
  if (!pic_eoi(pic_to_irq(stack->vector))) {
    printk(KERN_FAIL, "PIC: Failed to send EOI for %d (IRQ %d)", stack->vector, pic_to_irq(stack->vector));
    panic("Failed to send EOI");
  }
}

// disables all the interrupts
bool pic_disable() {
  if (!__pic_out8_all(false, 0xff)) {
    printk(KERN_FAIL, "PIC: Failed to mask interrupts, I/O failure\n");
    return false;
  }

  return true;
}

// enables all the interrupts
bool pic_enable() {
  if (!__pic_out8_all(false, 0)) {
    printk(KERN_FAIL, "PIC: Failed to unmask interrupts, I/O failure\n");
    return false;
  }

  return true;
}

/*

 * initialization of PICs is explained in the osdev link
 * it's done by sending multiple "Initialization Command Word"s (ICW)
 * each will send 8 bit data, data is used for setting up different info

 * i explained all the data values with comments, however i suggest you checkout
 * the datasheet for the 8259 PIC:
 * http://pdos.csail.mit.edu/6.828/2005/readings/hardware/8259A.pdf

*/
bool pic_init() {
  /*

   * ICW1 is sent over the communication channel
   * here's the data structure for ICW1:
   * - bit 0   = set as 1 if ICW4 is needed (which is needed to use x86 mode)
   * - bit 1   = set as 0 to use cascade mode
   * - bit 2   = ignored in x86 mode
   * - bit 3   = set as 0 to use edge triggered mode (see https://wiki.osdev.org/File:Edge_vs_level.png)
   * - bit 4   = random fucking 1 (not really, it's used to tell the PIC that this is a ICW1 and not an OCW)
   * - bit 5-7 = ignored in x86 mode

  */
  if (!__pic_out8_all(true, 1 | 1 << 4)) {
    printk(KERN_FAIL, "PIC: Failed to send ICW1, I/O failure\n");
    return false;
  }

  /*

   * ICW2 is sent over the data channel
   * here's the data structure for ICW2:
   * - bit 0-2 = ignored in x86 mode
   * - bit 3-7 = vector offset (intervals of 8)
   * we'll use the next 8 values of the offset for the slave

  */
  if (!out8_wait(PIC_MASTER_DATA, (PIC_VECTOR_OFFSET / 8) << 3) ||
      !out8_wait(PIC_SLAVE_DATA, ((PIC_VECTOR_OFFSET / 8) + ((PIC_IRQ_MAX + 1) / 8)) << 3)) {
    printk(KERN_FAIL, "PIC: Failed to send ICW2, I/O failure\n");
    return false;
  }

  /*

   * ICW3 is sent over the data channel as well

   * unlike the other ICWs, this time the data is different for the
   * slave and the master

   * for the master, each bit represents a IRQ port on the device
   * if a port has a slave connected, we set it to 1, so we'll set the bit 2

   * for the slave, only first 3 bits is used to specify the port
   * that the slave is connceted to, so we'll set it to 2

  */
  if (!out8_wait(PIC_MASTER_DATA, 1 << 2) || !out8_wait(PIC_SLAVE_DATA, 2)) {
    printk(KERN_FAIL, "PIC: Failed to send ICW3, I/O failure\n");
    return false;
  }

  /*

   * ICW4 is also sent over the data channel
   * here's the data structure for ICW4:
   * - bit 0   = set to 1 when using x86 mode
   * - bit 1-7 = nobody cares + L + ratio
   * jk there are some useful bits like the auto EOI bit but we don't need them

  */
  if (!__pic_out8_all(false, 1)) {
    printk(KERN_FAIL, "PIC: Failed to send ICW4, I/O failure\n");
    return false;
  }

  // setup the default PIC interrupt handler (to send EOI to all the interrupts)
  for (uint16_t i = 0; i < PIC_IRQ_TOTAL; i++)
    im_add_handler(pic_to_int(i), __pic_handler_default);

  return true;
}
