#include "core/ahci.h"
#include "fs/disk.h"

#include "util/math.h"
#include "util/mem.h"
#include "util/printk.h"

// ATAPI port command functions

#define AHCI_SATA_H2D     0x27
#define AHCI_SATA_COMMAND 1

enum ahci_atapi_cmd {
  AHCI_ATAPI_INQUIRY = 0x12,           // 6.4 INQUIRY command
#define AHCI_ATAPI_INQUIRY_DATA_MIN 36 // 6.4.2 Standard INQUIRY data
  AHCI_ATAPI_READ_CAPACITY = 0x25,     // 6.1.6 READ CAPACITY command
  AHCI_ATAPI_READ          = 0xA8,     // 6.2.4 READ(12) command
  AHCI_ATAPI_WRITE         = 0xAA,     // 6.2.13 WRITE(12) command
};

bool __ahci_atapi_cfis_setup(sata_fis_h2d_t *cfis) {
  if (NULL == cfis)
    return false;

  // we are gonna issue an PACKET command in the command FIS (7.18 PACKET – A0h, Packet)
  cfis->type    = AHCI_SATA_H2D;   // Register FIS - host to device
  cfis->command = AHCI_ATA_PACKET; // command
  cfis->c       = 1;               // 1 = command, 0 = control

  /*

   * DMA and DMADIR bit (7.18.4 DMA bit and DMADIR bit)
   * - first bit means we are using DMA
   * - second means the transfer is from device to host

  */
  cfis->feature_low = 1 | (1 << 2);

  return true;
}

bool ahci_atapi_port_read(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf) {
  // reset interrupt status
  data->port->is = UINT32_MAX;

  int8_t                  slot   = ahci_port_find_slot(data->port);
  struct ahci_cmd_header *header = ahci_port_get_slot(data->port, slot);
  struct ahci_cmd_table  *table  = NULL;

  if (NULL == header) {
    printk(KERN_DEBG, "AHCI: (0x%x) no available command slot for read command\n", data->port);
    return false;
  }

  // setup the command header which we obtained from the command list ("slot")
  if (NULL == (table = ahci_port_setup_header(
                   header, sizeof(sata_fis_h2d_t), false, sector_count * data->disk->sector_size, buf))) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to setup the header for read command\n", data->port, slot);
    return false;
  }

  // we are using ATAPI
  header->atapi = 1;

  // setup the command FIS
  __ahci_atapi_cfis_setup((void *)table->cfis);

  // now lets setup the command (https://wiki.osdev.org/ATAPI)
  table->acmd[0] = AHCI_ATAPI_READ;

  table->acmd[2] = (offset >> 24) & 0xff;
  table->acmd[3] = (offset >> 16) & 0xff;
  table->acmd[4] = (offset >> 8) & 0xff;
  table->acmd[5] = offset & 0xff;

  table->acmd[6] = (sector_count >> 24) & 0xff;
  table->acmd[7] = (sector_count >> 16) & 0xff;
  table->acmd[8] = (sector_count >> 8) & 0xff;
  table->acmd[9] = sector_count & 0xff;

  if (!ahci_port_issue_cmd(data->port, slot)) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to issue the read command\n", data->port, slot);
    return false;
  }

  printk(KERN_DEBG,
      "AHCI: (0x%x:%d) read command success: 0x%x,0x%x,0x%x,0x%x...\n",
      data->port,
      slot,
      buf[0],
      buf[1],
      buf[2],
      buf[3]);

  return true;
}

bool ahci_atapi_port_write(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf) {
  data->port->is = UINT32_MAX;

  int8_t                  slot   = ahci_port_find_slot(data->port);
  struct ahci_cmd_header *header = ahci_port_get_slot(data->port, slot);
  struct ahci_cmd_table  *table  = NULL;

  if (NULL == header) {
    printk(KERN_DEBG, "AHCI: (0x%x) no available command slot for write command\n", data->port);
    return false;
  }

  if (NULL == (table = ahci_port_setup_header(
                   header, sizeof(sata_fis_h2d_t), true, sector_count * data->disk->sector_size, buf))) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to setup the header for write command\n", data->port, slot);
    return false;
  }

  header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)table->cfis);

  table->acmd[0] = AHCI_ATAPI_WRITE;

  table->acmd[2] = (offset >> 24) & 0xff;
  table->acmd[3] = (offset >> 16) & 0xff;
  table->acmd[4] = (offset >> 8) & 0xff;
  table->acmd[5] = offset & 0xff;

  table->acmd[6] = (sector_count >> 24) & 0xff;
  table->acmd[7] = (sector_count >> 16) & 0xff;
  table->acmd[8] = (sector_count >> 8) & 0xff;
  table->acmd[9] = sector_count & 0xff;

  if (!ahci_port_issue_cmd(data->port, slot)) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to issue the write command\n", data->port, slot);
    return false;
  }

  printk(KERN_DEBG, "AHCI: (0x%x:%d) write command success\n", data->port, slot);

  return true;
}

bool __ahci_atapi_port_inquiry(ahci_port_data_t *data, struct ahci_cmd_header *header, int8_t slot) {
  struct ahci_cmd_table *table = NULL;

  /*

   * we are gonna use ATAPI INQUIRY command to get device type
   * 6.4.2 Standard INQUIRY data, explains the details of the data returned by this command
   * 1 byte should be enough for all the data we need, but there's a required min size

  */
  uint8_t inquiry_data[AHCI_ATAPI_INQUIRY_DATA_MIN];
  bzero(inquiry_data, sizeof(inquiry_data));

  if (NULL ==
      (table = ahci_port_setup_header(header, sizeof(sata_fis_h2d_t), false, sizeof(inquiry_data), inquiry_data))) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to setup the header for inquiry command\n", data->port, slot);
    return false;
  }
  header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)table->cfis);
  table->acmd[0] = AHCI_ATAPI_INQUIRY;
  table->acmd[3] = (sizeof(inquiry_data) >> 8) & 0xff;
  table->acmd[4] = sizeof(inquiry_data) & 0xff;

  if (!ahci_port_issue_cmd(data->port, slot)) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to issue the inquiry command\n", data->port, slot);
    return false;
  }

  printk(KERN_DEBG,
      "AHCI: (0x%x:%d) inquiry command success: 0x%x,0x%x,0x%x,0x%x...\n",
      data->port,
      slot,
      inquiry_data[0],
      inquiry_data[1],
      inquiry_data[2],
      inquiry_data[3]);

  // get device type (Table 82 — Peripheral device type)
  switch (inquiry_data[0] & 0x1f) {
  case 0x00: // Direct-access device (e.g., magnetic disk)
    data->disk->type = DISK_TYPE_HDD;
    break;

  case 0x0e: // Simplified direct-access device (e.g., magnetic disk)
    data->disk->type = DISK_TYPE_HDD;
    break;

  case 0x05: // CD/DVD device
    data->disk->type = DISK_TYPE_OPTICAL;
    break;
  }

  return true;
}

bool __ahci_atapi_port_capacity(ahci_port_data_t *data, struct ahci_cmd_header *header, int8_t slot) {
  struct ahci_cmd_table *table = NULL;

  uint8_t capacity_data[8];
  bzero(capacity_data, sizeof(capacity_data));

  if (NULL ==
      (table = ahci_port_setup_header(header, sizeof(sata_fis_h2d_t), false, sizeof(capacity_data), capacity_data))) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to setup the header for capacity command\n", data->port, slot);
    return false;
  }
  header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)table->cfis);
  table->acmd[0] = AHCI_ATAPI_READ_CAPACITY;

  if (!ahci_port_issue_cmd(data->port, slot)) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to issue the capacity command\n", data->port, slot);
    return false;
  }

  printk(KERN_DEBG,
      "AHCI: (0x%x:%d) capacity command success: 0x%x,0x%x,0x%x,0x%x...\n",
      data->port,
      slot,
      capacity_data[0],
      capacity_data[1],
      capacity_data[2],
      capacity_data[3]);

  data->disk->sector_size =
      capacity_data[7] | (capacity_data[6] << 8) | (capacity_data[5] << 16) | (capacity_data[4] << 24);
  data->disk->size =
      (capacity_data[3] | (capacity_data[2] << 8) | (capacity_data[1] << 16) | (capacity_data[0] << 24)) + 1;
  data->disk->size *= data->disk->sector_size;

  return true;
}

bool __ahci_atapi_port_mode_sense(ahci_port_data_t *data, struct ahci_cmd_header *header, int8_t slot) {
  struct ahci_cmd_table *table = NULL;

  uint8_t sense_data[254];
  bzero(sense_data, sizeof(sense_data));

  if (NULL == (table = ahci_port_setup_header(header, sizeof(sata_fis_h2d_t), false, sizeof(sense_data), sense_data))) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to setup the header for sense command\n", data->port, slot);
    return false;
  }
  header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)table->cfis);
  table->acmd[0] = 0x5A;
  // table->acmd[1] = 1 << 3;
  table->acmd[2] = 0x3F;
  // table->acmd[4] = 254;
  table->acmd[7] = (sizeof(sense_data) >> 8) & 0xff;
  table->acmd[8] = sizeof(sense_data) & 0xff;

  if (!ahci_port_issue_cmd(data->port, slot)) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to issue the sense command\n", data->port, slot);
    return false;
  }

  printk(KERN_DEBG,
      "AHCI: (0x%x:%d) sense command success: 0x%x,0x%x,0x%x,0x%x...\n",
      data->port,
      slot,
      sense_data[0],
      sense_data[1],
      sense_data[2],
      sense_data[3]);

  // check the write protect
  data->disk->read_only = (sense_data[3] & 1 << 7) == 1;

  return true;
}

bool ahci_atapi_port_info(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf) {
  data->port->is = UINT32_MAX;

  int8_t                  slot   = ahci_port_find_slot(data->port);
  struct ahci_cmd_header *header = ahci_port_get_slot(data->port, slot);

  if (NULL == header) {
    printk(KERN_DEBG, "AHCI: (0x%x) no available command slot for info commands\n", data->port);
    return false;
  }

  if (!__ahci_atapi_port_inquiry(data, header, slot)) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to obtain port info, inquiry command failed\n", data->port, slot);
    return false;
  }

  if (!__ahci_atapi_port_capacity(data, header, slot)) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to obtain port info, capacity command failed\n", data->port, slot);
    return false;
  }

  /*if(!__ahci_atapi_port_mode_sense(data, header, slot)){
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to obtain port info, sense command failed\n", data->port, slot);
    return false;
  }*/

  return true;
}
