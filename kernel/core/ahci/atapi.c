#include "core/ahci.h"

#include "util/mem.h"
#include "util/printk.h"
#include "util/string.h"
#include <stdint.h>

#define ahci_atapi_debg(f, ...) ahci_debg("(ATAPI 0x%x) " f, data->port, ##__VA_ARGS__)
#define ahci_atapi_info(f, ...) ahci_info("(ATAPI 0x%x) " f, data->port, ##__VA_ARGS__)
#define ahci_atapi_fail(f, ...) ahci_fail("(ATAPI 0x%x) " f, data->port, ##__VA_ARGS__)
#define ahci_atapi_warn(f, ...) ahci_warn("(ATAPI 0x%x) " f, data->port, ##__VA_ARGS__)

#define AHCI_SATA_H2D     0x27
#define AHCI_SATA_COMMAND 1

enum ahci_atapi_cmd {
  AHCI_ATAPI_INQUIRY = 0x12,           // 6.4 INQUIRY command
#define AHCI_ATAPI_INQUIRY_DATA_MIN 36 // 6.4.2 Standard INQUIRY data
  AHCI_ATAPI_READ_CAPACITY = 0x25,     // 6.1.6 READ CAPACITY command
  AHCI_ATAPI_READ          = 0xA8,     // 6.2.4 READ(12) command
  AHCI_ATAPI_WRITE         = 0xAA,     // 6.2.13 WRITE(12) command
};

bool __ahci_atapi_cfis_setup(struct sata_fis_h2d *cfis) {
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

/*

 * similar tot the ahci_sata_port_read, ahci_atapi_port_read reads specified amount
 * of sectors starting from the specified LBA

 * there no DMA commands for ATAPI, so it's normally not possible to read right to
 * the buffer however AHCI makes this possible

*/
int32_t ahci_atapi_port_read(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf) {
  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = buf,
      .data_size = sector_count * data->disk->sector_size,
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_atapi_debg("failed to setup the read command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port);
  cmd.header->write = 0;
  cmd.header->atapi = 1; // we are using ATAPI

  // setup the command FIS
  __ahci_atapi_cfis_setup((void *)cmd.table->cfis);

  // now lets setup the command (https://wiki.osdev.org/ATAPI)
  cmd.table->acmd[0] = AHCI_ATAPI_READ;

  cmd.table->acmd[2] = (lba >> 24) & 0xff;
  cmd.table->acmd[3] = (lba >> 16) & 0xff;
  cmd.table->acmd[4] = (lba >> 8) & 0xff;
  cmd.table->acmd[5] = lba & 0xff;

  cmd.table->acmd[6] = (sector_count >> 24) & 0xff;
  cmd.table->acmd[7] = (sector_count >> 16) & 0xff;
  cmd.table->acmd[8] = (sector_count >> 8) & 0xff;
  cmd.table->acmd[9] = sector_count & 0xff;

  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_atapi_debg("failed to issue the read command: %s", strerror(err));
    return err;
  }

  return 0;
}

/*

 * similar to the ahci_sata_port_write, ahci_atapi_port_write writes specified amount
 * of sectors starting from the specified LBA

 * as mentioned earlier, there are no DMA commands for ATAPI, so normally we wouldn't
 * be able write right from the buffer, however AHCI makes this possible

 * also the implementation is basically same with ahci_atapi_port_read, just uses a
 * different command

*/
int32_t ahci_atapi_port_write(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf) {
  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = buf,
      .data_size = sector_count * data->disk->sector_size,
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_atapi_debg("failed to setup the write command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port);
  cmd.header->write = 1;
  cmd.header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)cmd.table->cfis);

  cmd.table->acmd[0] = AHCI_ATAPI_WRITE;

  cmd.table->acmd[2] = (lba >> 24) & 0xff;
  cmd.table->acmd[3] = (lba >> 16) & 0xff;
  cmd.table->acmd[4] = (lba >> 8) & 0xff;
  cmd.table->acmd[5] = lba & 0xff;

  cmd.table->acmd[6] = (sector_count >> 24) & 0xff;
  cmd.table->acmd[7] = (sector_count >> 16) & 0xff;
  cmd.table->acmd[8] = (sector_count >> 8) & 0xff;
  cmd.table->acmd[9] = sector_count & 0xff;

  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_atapi_debg("failed to issue the write command: %s", strerror(err));
    return err;
  }

  return 0;
}

int32_t __ahci_atapi_port_inquiry(ahci_port_data_t *data) {
  /*

   * we are gonna use ATAPI INQUIRY command to get device type
   * 6.4.2 Standard INQUIRY data, explains the details of the data returned by this command
   * 1 byte should be enough for all the data we need, but there's a required min size

  */
  uint8_t inquiry_data[AHCI_ATAPI_INQUIRY_DATA_MIN];
  bzero(inquiry_data, sizeof(inquiry_data));

  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = inquiry_data,
      .data_size = sizeof(inquiry_data),
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_atapi_debg("failed to setup the inquiry command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port);
  cmd.header->write = 0;
  cmd.header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)cmd.table->cfis);
  cmd.table->acmd[0] = AHCI_ATAPI_INQUIRY;
  cmd.table->acmd[3] = (sizeof(inquiry_data) >> 8) & 0xff;
  cmd.table->acmd[4] = sizeof(inquiry_data) & 0xff;

  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_atapi_debg("failed to issue the inquiry command: %s", strerror(err));
    return err;
  }

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

  return 0;
}

int32_t __ahci_atapi_port_capacity(ahci_port_data_t *data) {
  uint8_t capacity_data[8];
  bzero(capacity_data, sizeof(capacity_data));

  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = capacity_data,
      .data_size = sizeof(capacity_data),
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_atapi_debg("failed to setup the capacity command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port);
  cmd.header->write = 0;
  cmd.header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)cmd.table->cfis);
  cmd.table->acmd[0] = AHCI_ATAPI_READ_CAPACITY;

  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_atapi_debg("failed to issue the capacity command: %s", strerror(err));
    return err;
  }

  data->disk->sector_size =
      capacity_data[7] | (capacity_data[6] << 8) | (capacity_data[5] << 16) | (capacity_data[4] << 24);
  data->disk->size =
      (capacity_data[3] | (capacity_data[2] << 8) | (capacity_data[1] << 16) | (capacity_data[0] << 24)) + 1;
  data->disk->size *= data->disk->sector_size;

  return 0;
}

int32_t __ahci_atapi_port_mode_sense(ahci_port_data_t *data) {
  uint8_t sense_data[254];
  bzero(sense_data, sizeof(sense_data));

  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = sense_data,
      .data_size = sizeof(sense_data),
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_atapi_debg("failed to setup the mode sense command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port);
  cmd.header->write = 0;
  cmd.header->atapi = 1;

  __ahci_atapi_cfis_setup((void *)cmd.table->cfis);
  cmd.table->acmd[0] = 0x5A;
  // table->acmd[1] = 1 << 3;
  cmd.table->acmd[2] = 0x3F;
  // table->acmd[4] = 254;
  cmd.table->acmd[7] = (sizeof(sense_data) >> 8) & 0xff;
  cmd.table->acmd[8] = sizeof(sense_data) & 0xff;

  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_atapi_debg("failed to issue the mode sense command: %s", strerror(err));
    return err;
  }

  // check the write protect
  data->disk->read_only = (sense_data[3] & 1 << 7) == 1;

  return 0;
}

/*

 * ahci_atapi_port_info uses different ATAPI commands to get information about the ATAPı devices
 * and saves it related structures

*/
int32_t ahci_atapi_port_info(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf) {
  int32_t err = 0;

  if ((err = __ahci_atapi_port_inquiry(data)) != 0) {
    // ahci_atapi_debg("failed to obtain the port info, inquiry command failed: %s", strerror(err));
    return err;
  }

  if ((err = __ahci_atapi_port_capacity(data)) != 0) {
    // ahci_atapi_debg("failed to obtain the port info, capacity command failed: %s", strerror(err));
    return err;
  }

  /*if(!__ahci_atapi_port_mode_sense(data, header, slot)){
    printk(KERN_DEBG, "AHCI: (0x%x:%d) failed to obtain port info, sense command failed\n", data->port, slot);
    return false;
  }*/

  return 0;
}
