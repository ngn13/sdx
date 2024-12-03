#include "core/mbr.h"
#include "core/disk.h"

#include "util/bit.h"
#include "util/printk.h"

#define mbr_debg(f, ...) pdebg("MBR: (0x%x) " f, disk, ##__VA_ARGS__)
#define mbr_fail(f, ...) pfail("MBR: (0x%x) " f, disk, ##__VA_ARGS__)

#define MBR_SIGNATURE 0xAA55
#define MBR_SIZE      512
#define MBR_LBA       0

struct mbr_part {
  uint8_t  attr;
  uint32_t start_chs : 24;
  uint8_t  type;
  uint32_t end_chs : 24;
  uint32_t start_lba;
  uint32_t sector_count;
} __attribute__((packed));

struct mbr_data {
  uint8_t         boostrap[440];
  uint32_t        id;
  uint16_t        reserved;
  struct mbr_part parts[4];
  uint16_t        signature;
} __attribute__((packed));

bool mbr_load(disk_t *disk) {
  disk_part_t     *dp   = NULL;
  struct mbr_part *part = NULL;
  struct mbr_data  mbr;

  if (!disk_do(disk, DISK_OP_READ, MBR_LBA, sizeof(mbr), (void *)&mbr)) {
    mbr_debg("failed to read the MBR data");
    return false;
  }

  if (mbr.signature != MBR_SIGNATURE) {
    mbr_debg("invalid MBR signature, disk is not MBR");
    return false;
  }

  mbr_debg("unique disk ID: %d", mbr.id);

  for (uint8_t i = 0; i < sizeof(mbr.parts) / sizeof(mbr.parts[0]); i++) {
    part = &mbr.parts[i];

    // check if the partition is valid or not
    if (part->start_chs == MBR_LBA || part->end_chs == MBR_LBA || part->start_lba == MBR_LBA || part->sector_count == 0)
      continue;

    // print the partion info
    mbr_debg("loading partition %d", i);
    mbr_debg("|- attributes: %d", part->attr);
    mbr_debg("|- type: %d", part->type);
    mbr_debg("|- start CHS: %d", part->start_chs);
    mbr_debg("|- end CHS: %d", part->end_chs);
    mbr_debg("|- start LBA: %d", part->start_lba);
    mbr_debg("`- sector count: %d", part->sector_count);

    // add the new disk partition
    if (NULL == (dp = disk_part_add(disk, part->start_lba, part->sector_count))) {
      pfail("failed to add a partition", disk);
      continue;
    }

    // load additional partition info and make the partition available
    dp->bootable  = bit_get(part->type, 7);
    dp->available = true;
  }

  return true;
}
