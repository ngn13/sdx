#include "core/ahci.h"

#include "util/mem.h"
#include "util/string.h"

#include "types.h"
#include "errno.h"

#define ahci_sata_debg(f, ...) ahci_debg("(SATA 0x%x) " f, data->port, ##__VA_ARGS__)
#define ahci_sata_info(f, ...) ahci_info("(SATA 0x%x) " f, data->port, ##__VA_ARGS__)
#define ahci_sata_fail(f, ...) ahci_fail("(SATA 0x%x) " f, data->port, ##__VA_ARGS__)
#define ahci_sata_warn(f, ...) ahci_warn("(SATA 0x%x) " f, data->port, ##__VA_ARGS__)

// SATA FIS types
enum ahci_sata_fis_type {
  SATA_FIS_REG_H2D   = 0x27, // Register FIS - host to device
  SATA_FIS_REG_D2H   = 0x34, // Register FIS - device to host
  SATA_FIS_DMA_ACT   = 0x39, // DMA activate FIS - device to host
  SATA_FIS_DMA_SETUP = 0x41, // DMA setup FIS - bidirectional
  SATA_FIS_DATA      = 0x46, // Data FIS - bidirectional
  SATA_FIS_BIST      = 0x58, // BIST activate FIS - bidirectional
  SATA_FIS_PIO_SETUP = 0x5F, // PIO setup FIS - device to host
  SATA_FIS_DEV_BITS  = 0xA1, // Set device bits FIS - device to host
};

struct ahci_sata_cmd {
  uint8_t cmd;         // SATA/ATA command
  uint8_t device;      // device register value
  bool    use_sectors; // if set to true, then the sector count register is required
  bool    use_lba;     // if set to true, then LBA register is required
};

// A.11.7.4 MODEL NUMBER field
#define AHCI_MODEL_LEN 40

struct ahci_sata_cmd ahci_sata_cmds[] = {
    // for some commands 6th device bit "Shall be set to one" (for example see 7.22 READ DMA EXT – 25h, DMA)
    {.cmd = AHCI_ATA_READ_DMA_EXT,    .use_sectors = true,  .device = (1 << 6), .use_lba = true },
    {.cmd = AHCI_ATA_WRITE_DMA_EXT,   .use_sectors = true,  .device = (1 << 6), .use_lba = true },
    {.cmd = AHCI_ATA_IDENTIFY_DEVICE, .use_sectors = false, .device = 0,        .use_lba = false},
};

struct ahci_sata_cmd *__ahci_sata_find_props(uint8_t cmd) {
  for (uint8_t i = 0; i < sizeof(ahci_sata_cmds) / sizeof(struct ahci_sata_cmd); i++)
    if (ahci_sata_cmds[i].cmd == cmd)
      return &ahci_sata_cmds[i];
  return NULL;
}

bool __ahci_sata_new(
    ahci_port_data_t *data, struct sata_fis_h2d *fis, enum ahci_ata_cmd cmd, uint64_t lba, uint64_t sector_count) {
  // find the command properites
  struct ahci_sata_cmd *c = __ahci_sata_find_props(cmd);

  // check the FIS pointer
  if (NULL == fis || NULL == c)
    return false;

  // clear out the FIS
  bzero(fis, sizeof(struct sata_fis_h2d));

  // the FIS type, command/control selection and the command
  fis->type    = SATA_FIS_REG_H2D;
  fis->c       = SATA_FIS_H2D_COMMAND;
  fis->command = cmd;

  // the device register value for the command
  fis->device = c->device;

  if (c->use_lba) {
    // setup the LBA(48)
    fis->lba0 = (uint8_t)(lba & 0xFF);         // 0-7
    fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);  // 8-15
    fis->lba2 = (uint8_t)((lba >> 16) & 0xFF); // 16-23
    fis->lba3 = (uint8_t)((lba >> 24) & 0xFF); // 24-31
    fis->lba4 = (uint8_t)((lba >> 32) & 0xFF); // 32-39
    fis->lba5 = (uint8_t)((lba >> 40) & 0xFF); // 40-47
  }

  if (c->use_sectors)
    // setup the sector count register
    fis->count = sector_count;

  return true;
}

/*

 * ahci_sata_port_read reads specified amount of sectors starting from the
 * specified LBA it uses READ_DMA so we read right into the specified buffer

*/
int32_t ahci_sata_port_read(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf) {
  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = buf,
      .data_size = sector_count * data->disk->sector_size,
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  // setup the command
  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_sata_debg("failed to setup the read command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port); // reset interrupt status
  cmd.header->write = 0;          // this is a read operation

  // now lets setup the command
  if (!__ahci_sata_new(data, (void *)cmd.table->cfis, AHCI_ATA_READ_DMA_EXT, lba, sector_count)) {
    ahci_sata_debg("failed to create the read command FIS");
    return -EFAULT;
  }

  // and last but not least, issue the command
  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_sata_debg("failed to issue the read command: %s", strerror(err));
    return err;
  }

  return 0;
}

/*

 * ahci_sata_port_write writes specified amount of sectors starting from the
 * specified LBA it uses WRITE_DMA so we write right from the buffer

 * for the implementation, mainly it's similar to ahci_sata_port_read however
 * this time, we'll setup a different command

*/
int32_t ahci_sata_port_write(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf) {
  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = buf,
      .data_size = sector_count * data->disk->sector_size,
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_sata_debg("failed to setup the write command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port);
  cmd.header->write = 1;

  if (!__ahci_sata_new(data, (void *)cmd.table->cfis, AHCI_ATA_WRITE_DMA_EXT, lba, sector_count)) {
    ahci_sata_debg("failed to create the write command FIS");
    return -EFAULT;
  }

  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_sata_debg("failed to issue the write command: %s", strerror(err));
    return err;
  }

  return 0;
}

/*

 * ahci_sata_port_info uses the IDENTIFY_DEVICE command to get information about
 * the device, and saves it to related structures

*/
int32_t ahci_sata_port_info(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf) {
  // buffer used to store IDENTIFY command output
  uint8_t info[AHCI_ATA_IDENTIFY_DEVICE_DATA_SIZE];
  bzero(info, sizeof(info));

  int32_t    err = 0;
  ahci_cmd_t cmd = {
      .vaddr     = data->vaddr,
      .port      = data->port,
      .data      = info,
      .data_size = sizeof(info),
      .fis_size  = sizeof(struct sata_fis_h2d),
  };

  if ((err = ahci_cmd_setup(&cmd)) != 0) {
    ahci_sata_debg("failed to setup the identify command: %s", strerror(err));
    return err;
  }

  ahci_port_reset_is(data->port);
  cmd.header->write = 0;

  if (!__ahci_sata_new(data, (void *)cmd.table->cfis, AHCI_ATA_IDENTIFY_DEVICE, 0, 0)) {
    ahci_sata_debg("failed to create the identify command FIS");
    return -EFAULT;
  }

  if ((err = ahci_cmd_issue(&cmd)) != 0) {
    ahci_sata_debg("failed to issue the identify command: %s", strerror(err));
    return err;
  }

  // extract the model
  char model[AHCI_MODEL_LEN + 1];
  bzero(model, sizeof(model));

  /*

   * the model number string is parsed in a weird way
   * see the following sections for more information

   * 7.12.7.14 Words 27..46: Model number
   * 3.3.10 ATA string convention

  */
  for (uint8_t i = 0; i < AHCI_MODEL_LEN; i += 2) {
    // 54 is the byte offset for "Model number (see 7.12.7.14)"
    model[i]     = info[54 + i + 1];
    model[i + 1] = info[54 + i];
  }

  // is there a better way to do this??
  if (strstr(model, "HARDDISK") || strstr(model, "HDD"))
    data->disk->type = DISK_TYPE_HDD;
  else if (strstr(model, "SOLIDSTATE") || strstr(model, "SSD"))
    data->disk->type = DISK_TYPE_SSD;
  else if (strstr(model, "CD") || strstr(model, "DVD"))
    data->disk->type = DISK_TYPE_OPTICAL;

  if ((((uint16_t *)info)[100] >> 10) & 1)
    // Number of User Addressable Logical Sectors (QWord) (see 7.12.7.53)
    data->disk->size = (uint64_t)((uint16_t *)info)[102] << 32 | (uint64_t)((uint16_t *)info)[101] << 16 |
                       (uint64_t)((uint16_t *)info)[100];
  else
    // Total number of user addressable logical sectors for 28-bit commands (DWord) (see 7.12.7.22)
    data->disk->size = (uint64_t)((uint16_t *)info)[61] << 16 | (uint64_t)((uint16_t *)info)[60];

  data->disk->size *= sizeof(uint64_t) * data->disk->sector_size;

  // TODO: somehow obtain the sector size as well

  return 0;
}
