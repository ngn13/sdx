#include "core/ahci.h"
#include "core/disk.h"
#include "core/pci.h"

#include "util/bit.h"
#include "util/mem.h"
#include "util/printk.h"

#include "mm/heap.h"
#include "mm/vmm.h"

#include "types.h"
#include "errno.h"

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

typedef int32_t ahci_op_func_t(ahci_port_data_t *, uint64_t, uint64_t, uint8_t *);

struct ahci_protocol_func {
  disk_op_t       op;
  ahci_op_func_t *func;
  ahci_protocol_t protocol;
  const char     *name;
  bool            needs_buffer;
};

struct ahci_protocol_func ahci_protocol_funcs[] = {
    // SATA protocol functions
    {.protocol        = AHCI_PROTOCOL_SATA,
     .op           = DISK_OP_READ,
     .func         = ahci_sata_port_read,
     .name         = "SATA read",
     .needs_buffer = true},
    {.protocol        = AHCI_PROTOCOL_SATA,
     .op           = DISK_OP_WRITE,
     .func         = ahci_sata_port_write,
     .name         = "SATA write",
     .needs_buffer = true},
    {.protocol        = AHCI_PROTOCOL_SATA,
     .op           = DISK_OP_INFO,
     .func         = ahci_sata_port_info,
     .name         = "SATA info",
     .needs_buffer = false},

    // ATAPI protocol functions
    {.protocol        = AHCI_PROTOCOL_ATAPI,
     .op           = DISK_OP_READ,
     .func         = ahci_atapi_port_read,
     .name         = "ATAPI read",
     .needs_buffer = true},
    {.protocol        = AHCI_PROTOCOL_ATAPI,
     .op           = DISK_OP_WRITE,
     .func         = ahci_atapi_port_write,
     .name         = "ATAPI write",
     .needs_buffer = true},
    {.protocol        = AHCI_PROTOCOL_ATAPI,
     .op           = DISK_OP_INFO,
     .func         = ahci_atapi_port_info,
     .name         = "ATAPI info",
     .needs_buffer = false},

    {.func = NULL},
};

char *__ahci_port_protocol(ahci_port_data_t *data) {
  switch (data->protocol) {
  case AHCI_PROTOCOL_SATA:
    return "SATA";
  case AHCI_PROTOCOL_ATAPI:
    return "ATAPI";
  }

  return "unknown";
}

int32_t ahci_do(ahci_port_data_t *data, disk_op_t op, uint64_t lba, uint64_t sector_count, uint8_t *buffer) {
  if (NULL == data)
    return -EINVAL;

  struct ahci_protocol_func *pf = NULL;

  for (pf = &ahci_protocol_funcs[0]; pf->func != NULL; pf++) {
    if (pf->op != op || pf->protocol != data->protocol)
      continue;

    if (pf->needs_buffer && (NULL == buffer || sector_count <= 0)) {
      ahci_fail("%s operation failed on port 0x%x, no buffer provided", pf->name, data->port);
      return -EINVAL;
    }

    // ahci_debg("performing %s operation on port 0x%x", pf->name, data->port);
    return pf->func(data, lba, sector_count, buffer);
  }

  ahci_fail("unknown %s operation on port 0x%x: %d", __ahci_port_protocol(data), data->port, op);
  return -EINVAL;
}

int32_t ahci_init(pci_device_t *dev) {
  uint64_t abar = pci_device_read32(dev, 0x24);

  // lower 13 bits are not part of the base address (page 18 in da spec)
  ahci_mem_t *base = (void *)(abar & 0xfffffff0);

  // map the base address after calculating max page count we'll need
  uint64_t base_page_count = vmm_calc(sizeof(ahci_mem_t));
  base                     = vmm_map_paddr((uint64_t)base, base_page_count, VMM_ATTR_NO_CACHE | VMM_ATTR_SAVE);
  ahci_debg("mapped ABAR at 0x%p to 0x%p", vmm_resolve((void *)base), base);

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

  ahci_info("HBA at 0x%p supports version %d.%d", base, (base->vs >> 16) & 0xFFFF, base->vs & 0xFFFF);
  ahci_info("enumerating %u ports", sizeof(base->ports) / sizeof(base->ports[0]));

  ahci_port_data_t *port_data  = NULL;
  void             *port_vaddr = NULL;

  for (uint8_t i = 0; i < sizeof(base->pi) * 8; i++) {
    if (((base->pi >> i) & 1) != 1)
      continue;

    if (!ahci_port_is_connected(&base->ports[i]))
      continue;

    if ((port_vaddr = ahci_port_setup(&base->ports[i])) == NULL)
      continue;

    // allocate space for the new port data
    port_data = heap_alloc(sizeof(ahci_port_data_t));
    bzero(port_data, sizeof(ahci_port_data_t));

    // save the port data
    port_data->port  = &base->ports[i];
    port_data->hba   = base;
    port_data->vaddr = port_vaddr;
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

    ahci_info("found an available port at index %u", port_data->index);
    pinfo("      |- HBA: 0x%p", port_data->hba);
    pinfo("      |- Signature: 0x%x (%s)", port_data->port->sig, __ahci_port_protocol(port_data));
    pinfo("      |- Address: 0x%p", port_data);
    pinfo("      `- Vaddr: 0x%p", port_data->vaddr);

    // add the disk and load the partitions
    port_data->disk = disk_add(DISK_CONTROLLER_AHCI, port_data);
    disk_part_scan(port_data->disk);
  }

  // add interrupt handler
  // im_add_handler(dev->int_line + PIC_VECTOR_OFFSET, ahci_handle_int);

  // enable interrupts
  // bit_set(base->ghc, 1, 1);

  return 0;
}
