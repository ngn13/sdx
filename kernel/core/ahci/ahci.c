#include "core/ahci.h"
#include "core/im.h"
#include "core/pci.h"

#include "fs/disk.h"
#include "mm/pm.h"
#include "mm/vmm.h"

#include "util/bit.h"
#include "util/math.h"
#include "util/mem.h"
#include "util/printk.h"

// clang-format off

/*

 * AHCI driver for SATA
 * this driver is intially loaded by the PCI

 * here is a list of all the referenced specs
 * - AHCI: https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf
 * - SATA: https://sata-io.org/system/files/specifications/SerialATA_Revision_3_1_Gold.pdf
 * - ACS: https://files.ngn.tf/ATA_ATAPI_Command_Set_3.pdf
 * - SCSI (primary commands): https://files.ngn.tf/SCSI_Primary_Commands_3_rev21b.pdf 
 * - SCSI (block commands): https://files.ngn.tf/SCSI_Block_Commands_rev8c.pdf

*/

// clang-format on

pci_driver_t ahci_driver = {
    .name = "AHCI",
    .init = ahci_init,

    .vendor_id = PCI_VENDOR_ANY,
    .device_id = PCI_DEVICE_ANY,

    .class    = PCI_CLASS_STORAGE,
    .subclass = 6, // SATA, see https://wiki.osdev.org/PCI#Class_Codes
    .type     = PCI_TYPE_GENERAL,
};

// 3.1.10 Offset 24h: CAP2 – HBA Capabilities Extended
enum ahci_cap2_bits {
  AHCI_CAP2_BOH = 0, // BIOS/OS handoff
  // ...
};

// 3.1.11 Offset 28h: BOHC – BIOS/OS Handoff Control and Status
enum ahci_bohc_bits {
  AHCI_BOHC_BOS = 0, // BIOS owned semaphore
  AHCI_BOHC_OOS = 1, // OS owned semaphore
  // ...
};

typedef bool ahci_op_func_t(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf);

struct ahci_protocol_func {
  disk_op_t       op;
  ahci_op_func_t *func;
  ahci_protocol_t protocol;
  bool            use_buf;
  bool            use_sector_count;
};

struct ahci_protocol_func ahci_protocol_map[] = {
    // SATA protocol functions
    {.protocol            = AHCI_PROTOCOL_SATA,
     .op               = DISK_OP_READ,
     .func             = ahci_sata_port_read,
     .use_buf          = true,
     .use_sector_count = true },
    {.protocol            = AHCI_PROTOCOL_SATA,
     .op               = DISK_OP_WRITE,
     .func             = ahci_sata_port_write,
     .use_buf          = true,
     .use_sector_count = true },
    {.protocol            = AHCI_PROTOCOL_SATA,
     .op               = DISK_OP_INFO,
     .func             = ahci_sata_port_info,
     .use_buf          = false,
     .use_sector_count = false},

    // ATAPI protocol functions
    {.protocol            = AHCI_PROTOCOL_ATAPI,
     .op               = DISK_OP_READ,
     .func             = ahci_atapi_port_read,
     .use_buf          = true,
     .use_sector_count = true },
    {.protocol            = AHCI_PROTOCOL_ATAPI,
     .op               = DISK_OP_WRITE,
     .func             = ahci_atapi_port_write,
     .use_buf          = true,
     .use_sector_count = true },
    {.protocol            = AHCI_PROTOCOL_ATAPI,
     .op               = DISK_OP_INFO,
     .func             = ahci_atapi_port_info,
     .use_buf          = false,
     .use_sector_count = false},
};

bool ahci_port_do(ahci_port_data_t *data, disk_op_t op, uint64_t offset, uint64_t sector_count, uint8_t *buf) {
  if (NULL == data)
    return false;

  for (uint8_t i = 0; i < sizeof(ahci_protocol_map) / sizeof(struct ahci_protocol_func); i++) {
    if (ahci_protocol_map[i].op != op)
      continue;

    if (ahci_protocol_map[i].protocol != data->protocol)
      continue;

    if (ahci_protocol_map[i].use_buf && NULL == buf) {
      printk(KERN_DEBG, "AHCI: (0x%x) operation failed, required buffer not provided\n", data->port);
      return false;
    }

    if (ahci_protocol_map[i].use_sector_count && sector_count == 0) {
      printk(KERN_DEBG, "AHCI: (0x%x) operation failed, bad sector count\n", data->port);
      return false;
    }

    return ahci_protocol_map[i].func(data, offset, sector_count, buf);
  }

  return false;
}

bool ahci_init(pci_device_t *dev) {
  uint32_t abar = pci_device_read32(dev, 0x24);
  // lower 13 bits are not part of the base address (page 18 in da spec)
  ahci_mem_t *base = (ahci_mem_t *)(uint64_t)(abar & 0xfffffff0);

  // make sure the base address is mapped
  pm_extend((uint64_t)base + sizeof(ahci_mem_t));
  pm_set((uint64_t)base, div_ceil(sizeof(ahci_mem_t), PM_PAGE_SIZE), PM_ENTRY_FLAGS_DEFAULT | PM_ENTRY_FLAG_PCD);

  // if BOHC is implemented and the BOHC indicates that the HBA is not OS owned, we'll own it
  if (bit_get(base->cap2, AHCI_CAP2_BOH) == 1 && bit_get(base->bohc, AHCI_BOHC_OOS) != 1) {
    bit_set(base->bohc, AHCI_BOHC_OOS, 1);

    while (bit_get(base->bohc, AHCI_BOHC_OOS) != 1 || bit_get(base->bohc, AHCI_BOHC_BOS) == 1)
      continue;
  }

  // HBA reset (page 26 in da spec)
  bit_set(base->ghc, 0, 1);

  while (bit_get(base->ghc, 0) == 1)
    continue;

  // enable AHCI
  bit_set(base->ghc, 31, 1);

  // disable interrupts & clear interrupt status
  bit_set(base->ghc, 1, 0);
  base->is = UINT32_MAX;

  printk(KERN_INFO,
      "AHCI: (0x%x) HBA supports version %d.%d, enumerating ports\n",
      base,
      (base->vs >> 16) & 0xFFFF,
      base->vs & 0xFFFF);

  ahci_port_data_t *port_data = NULL;

  for (uint8_t i = 0; i < sizeof(base->pi) * 8; i++) {
    if (((base->pi >> i) & 1) != 1)
      continue;

    if (!ahci_port_check(&base->ports[i]))
      continue;

    if (!ahci_port_init(&base->ports[i]))
      continue;

    // allocate space for the new port data
    port_data = vmm_alloc(sizeof(struct ahci_port_data));
    bzero(port_data, sizeof(struct ahci_port_data));

    // save the port data
    port_data->base  = base;
    port_data->port  = &base->ports[i];
    port_data->index = i;

    // setup port functions
    switch (port_data->port->sig) {
    case AHCI_SIGNATURE_SATA:
      port_data->protocol = AHCI_PROTOCOL_SATA;
      break;

    case AHCI_SIGNATURE_ATAPI:
      port_data->protocol = AHCI_PROTOCOL_ATAPI;
      break;
    }

    printk(KERN_INFO,
        "AHCI: (0x%x:%d) Address: 0x%x Signature: 0x%x\n",
        base,
        port_data->index,
        port_data->port,
        port_data->port->sig);

    // add the disk
    port_data->disk = disk_add(DISK_CONTROLLER_AHCI, port_data);
    disk_scan(port_data->disk);
  }

  // add interrupt handler
  // im_add_handler(dev->int_line + PIC_VECTOR_OFFSET, ahci_handle_int);

  // enable interrupts
  // bit_set(base->ghc, 1, 1);

  return true;
}