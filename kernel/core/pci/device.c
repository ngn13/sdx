#include "core/pci.h"
#include "util/mem.h"
#include "util/printk.h"

// driver headers
#include "core/ahci.h"

// driver list
pci_driver_t *pci_drivers[] = {
    &ahci_driver,
};

#define PCI_DRIVER_COUNT    sizeof(pci_drivers) / sizeof(pci_driver_t *)
#define __pci_get_driver(i) pci_drivers[i]

void pci_device_load(pci_device_t *d, uint8_t bus, uint8_t slot, uint8_t func) {
#define read16(offset) pci_read16(bus, slot, func, offset)
#define read8(offset)  pci_read8(bus, slot, func, offset)

  bzero(d, sizeof(pci_device_t));

  // see https://wiki.osdev.org/PCI#Header_Type_0x0
  d->vendor_id = read16(0);
  d->device_id = read16(2);

  d->command = read16(4);
  d->status  = read16(6);

  d->revision_id = read8(8);
  d->progif      = read8(9);
  d->subclass    = read8(10);
  d->class       = read8(11);

  d->type = read8(14);
  d->bist = read8(15);

  d->int_line = read8(60);

  d->bus  = bus;
  d->slot = slot;
  d->func = func;
}

bool pci_device_init(pci_device_t *d) {
  pci_driver_t *cur = NULL;

  for (uint8_t i = 0; i < PCI_DRIVER_COUNT; i++) {
    if ((cur = __pci_get_driver(i)) == NULL)
      continue;

    if (cur->vendor_id != PCI_VENDOR_ANY && cur->vendor_id != d->vendor_id) {
      cur = NULL;
      continue;
    }

    if (cur->device_id != PCI_DEVICE_ANY && cur->device_id != d->device_id) {
      cur = NULL;
      continue;
    }

    if (cur->type != PCI_TYPE_ANY && cur->class != d->class) {
      cur = NULL;
      continue;
    }

    if (cur->class != PCI_CLASS_ANY && cur->class != d->class) {
      cur = NULL;
      continue;
    }

    if (cur->subclass != PCI_SUBCLASS_ANY && cur->subclass != d->subclass) {
      cur = NULL;
      continue;
    }

    break;
  }

  // device does not have a driver
  if (NULL == cur)
    return false;

  printk(KERN_INFO,
      "PCI: loading %s for 0x%x:0x%x (bus %d slot %d function %d)\n",
      cur->name,
      d->vendor_id,
      d->device_id,
      d->bus,
      d->slot,
      d->func);

  return cur->init != NULL ? cur->init(d) : true;
}
