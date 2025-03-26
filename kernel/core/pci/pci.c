#include "core/pci.h"
#include "mm/heap.h"

#include "util/io.h"
#include "util/mem.h"
#include "util/printk.h"
#include <stdint.h>

/*

 * peripheral component interconnect (PCI) functions

 * PCI is used to access different useful slots, such as storage devices,
 * video cards, network cards sound cards and more

 * nowadays PCIe is mostly preffered over PCI, but for compability
 * the good ol' PCI is still support by most slots

 * to learn more about PCI, see:
 * https://wiki.osdev.org/PCI

*/

#define PCI_ADDRESS_PORT 0xCF8
#define PCI_DATA_PORT    0xCFC

/*

 * each PCI have 256 different "buses", each bus can contain 32 "slots" (called "slot" in the code)
 * each slot have their own seperate 8 "functions", which are used for different operations
 * but in practice they act as different slots for the system

 * each function have different "registers", a register is 32 bits long
 * PCI uses 256 bytes for this registers so there are 8 registers
 * PCIe expands this to 4096 bytes (128 register)

 * registers are seperated into different "offsets", each offset represents
 * a dword, so 8 bytes, which means a register contains 4 offsets
 * however the offsets are contiguous, so offset 0-4 represents the first register
 * offset 4-8 represents the second register etc.

*/

#define PCI_BUS_COUNT  256 // 256 buses, 255 max
#define PCI_SLOT_COUNT 32  // 32 devices, 31 max
#define PCI_FUNC_COUNT 8   // 8 functions, 7 max

/*

 * a structure to maintain PCI devices
 * stores the device list and count

*/

struct pci_data {
  pci_device_t *list;
  uint32_t      count;
} data;

/*

 * so an address is 32 bits long, and contains the following information:
 * - bits 0-7  : offset
 * - bits 8-10 : function
 * - bits 11-15: slot
 * - bits 16-23: bus
 * - bits 24-30: reserved (zero)
 * - bit 31    : idk but its set to 1

*/
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {

  uint32_t addr = 1 << 31 | (bus << 16) | (slot << 11) | (func << 8) | offset;

  if (!out32(PCI_ADDRESS_PORT, addr))
    return 0;

  // we are reading the entire thing so we don't really care about the offset
  return in32(PCI_DATA_PORT);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t addr = 1 << 31 | (bus << 16) | (slot << 11) | (func << 8) | offset;

  if (!out32(PCI_ADDRESS_PORT, addr))
    return 0;

  return in16(PCI_DATA_PORT + (offset & 2));
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t addr = 1 << 31 | (bus << 16) | (slot << 11) | (func << 8) | offset;

  if (!out32(PCI_ADDRESS_PORT, addr))
    return 0;

  return in8(PCI_DATA_PORT + (offset & 3));
}

bool pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t data) {
  uint32_t addr = 1 << 31 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc);
  return out32(PCI_ADDRESS_PORT, addr) && out32(PCI_DATA_PORT, data);
}

bool pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data) {
  uint32_t addr = 1 << 31 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc);
  return out32(PCI_ADDRESS_PORT + (offset & 2), addr) && out16(PCI_DATA_PORT + (offset & 2), data);
}

bool pci_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t data) {
  uint32_t addr = 1 << 31 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc);
  return out32(PCI_ADDRESS_PORT + (offset & 3), addr) && out8(PCI_DATA_PORT + (offset & 3), data);
}

bool pci_exists(uint8_t bus, uint8_t slot, uint8_t func) {
  /*

   * when you access a device that does not exist, host bridge returns all ones while reading
   * and there's no vendor with the ID 0xffff so we can use this to check if the device exists

  */
  return pci_read16(bus, slot, func, 0) != 0xffff;
}

void __pci_enum_single(uint8_t bus, uint8_t slot, uint8_t func) {
  if (func >= PCI_FUNC_COUNT)
    return;

  if (!pci_exists(bus, slot, func))
    goto next;

  pci_device_t *cur       = NULL;
  uint64_t      list_size = sizeof(pci_device_t) * (++data.count);

  if (NULL == data.list)
    data.list = heap_alloc(list_size);
  else
    data.list = heap_realloc(data.list, list_size);

  cur = &data.list[data.count - 1];
  pci_device_load(cur, bus, slot, func);

  if (func == 0 && (cur->type & 0x80) != 0)
    return __pci_enum_single(bus, slot, ++func);

next:
  if (func != 0)
    return __pci_enum_single(bus, slot, ++func);
}

// dirty iterative and recursive PCI enumeration
void pci_enum() {
  uint16_t bus_cur = 0, slot_cur = 0, func_cur = 0;

  for (bus_cur = 0; bus_cur < PCI_BUS_COUNT; bus_cur++)
    for (slot_cur = 0; slot_cur < PCI_SLOT_COUNT; slot_cur++)
      __pci_enum_single(bus_cur, slot_cur, 0);

  printk(KERN_INFO, "PCI: enumerated %d devices\n", data.count);

  for (uint16_t i = 0; i < data.count; i++) {
    printk(KERN_INFO,
        i == data.count - 1 ? "     `- Vendor: 0x%x Device: 0x%x Class: %d Subclass: %d\n"
                            : "     |- Vendor: 0x%x Device: 0x%x Class: %d Subclass: %d\n",
        data.list[i].vendor_id,
        data.list[i].device_id,
        data.list[i].class,
        data.list[i].subclass);
  }
}

// initialize supported and available PCI devices
int32_t pci_init() {
  bzero(&data, sizeof(struct pci_data));

  // enumerate all the PCI devices
  pci_enum();

  // load the device drivers
  for (uint16_t i = 0; i < data.count; i++)
    pci_device_init(&data.list[i]);

  return 0;
}
