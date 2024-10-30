#pragma once
#include "types.h"

/*

 * peripheral component interconnect (PCI) definitions
 * see: core/pci

*/
typedef enum pci_device_class {
  // https://wiki.osdev.org/PCI#Class_Codes
  PCI_CLASS_WTFISTHIS = 0,
  PCI_CLASS_STORAGE   = 1,
  PCI_CLASS_NETWORK   = 2,
  PCI_CLASS_DISPLAY   = 3,
  PCI_CLASS_MEDIA     = 4,
  PCI_CLASS_MEMORY    = 5,
  PCI_CLASS_BRIDGE    = 6,
} pci_device_class_t;

typedef struct pci_device {
  uint8_t bus, slot, func;

  // common header fields (https://wiki.osdev.org/PCI#Common_Header_Fields)

  // register 0, offset 0
  uint16_t vendor_id; // vendor ID (see https://pcisig.com/membership/member-companies)
  uint16_t device_id; // ID allocated by the vendor

  // regitser 1, offset 4
  uint16_t command;
  uint16_t status;

  // register 2, offset 8
  uint8_t revision_id;
  uint8_t progif;
  uint8_t subclass;
  uint8_t class;

  // register 3, offset 12
  uint8_t type;
  uint8_t bist;
  // uint8_t latency_timer;
  // uint8_t cache_line_size;

  // register 15, offset 60
  uint8_t int_line;
  // uint8_t int_pin;
  // uint8_t min_grant;
  // uint8_t max_latency;
} pci_device_t;

typedef enum pci_device_type {
  PCI_TYPE_GENERAL       = 0,
  PCI_TYPE_PCI_2_PCI     = 1,
  PCI_TYPE_PCI_2_CARDBUS = 2,
} pci_device_type_t;

typedef struct pci_driver {
  // driver name
  char *name;

  // used to match with a device
  uint16_t vendor_id;
  uint16_t device_id;
  uint8_t  subclass;
  uint8_t class;
  uint8_t type;

#define PCI_VENDOR_ANY   0xffff
#define PCI_DEVICE_ANY   0xffff
#define PCI_SUBCLASS_ANY 0xff
#define PCI_CLASS_ANY    0xff
#define PCI_TYPE_ANY     0xff

  // init function for the driver
  bool (*init)(pci_device_t *d);
} pci_driver_t;

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read8(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);

bool pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t data);
bool pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data);
bool pci_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t data);

void pci_device_load(
    pci_device_t *d, uint8_t bus, uint8_t slot, uint8_t func); // load a device data from a specific address
bool pci_device_init(pci_device_t *d);                         // init a device, with the driver(s)
#define pci_device_read32(d, o) pci_read32(d->bus, d->slot, d->func, o)
#define pci_device_read16(d, o) pci_read16(d->bus, d->slot, d->func, o)
#define pci_device_read8(d, o)  pci_read8(d->bus, d->slot, d->func, o)

void pci_init();                                          // initialize PCI devices
void pci_enum();                                          // enumerate the PCI devices
bool pci_exsits(uint8_t bus, uint8_t slot, uint8_t func); // check if a device exists at specific address
