#include "fs/gpt.h"
#include "fs/disk.h"

#include "util/bit.h"
#include "util/math.h"
#include "util/mem.h"
#include "util/printk.h"

#define gpt_debg(f)        pdebgf("GPT: (0x%x) " f, disk)
#define gpt_debgf(f, a...) pdebgf("GPT: (0x%x) " f, disk, a)

#define GPT_SIGNATURE  0x5452415020494645 // signature for the partition table header
#define GPT_PROTECTIVE 0xee               // PMBR OS type
#define GPT_LBA        1

struct gpt_table_header {
  uint64_t signature;      // 8
  uint32_t revision;       // 12
  uint32_t header_size;    // 16
  uint32_t reserved0;      // 20
  uint32_t crc32_checksum; // 24
  uint64_t lba_header;     // 32
  uint64_t lba_alternate;  // 40
  uint64_t first_block;    // 48
  uint64_t last_block;     // 56
  uint8_t  guid[16];       // 72
  uint64_t lba_array;      // 80
  uint32_t entry_count;    // 84
  uint32_t entry_size;     // 88
  uint32_t crc32;          // 92
} __attribute__((packed));

struct gpt_part_entry {
  uint8_t  type[16];
  uint8_t  guid[16];
  uint64_t start_lba;
  uint64_t end_lba;
  uint64_t attr;
  uint8_t  name[];
} __attribute__((packed));

void __gpt_load_entry(disk_t *disk, struct gpt_part_entry *part, uint64_t indx) {
  uint64_t type[2] = {*(uint64_t *)(&part->type[0]), *(uint64_t *)(&part->type[8])};
  uint64_t guid[2] = {*(uint64_t *)(&part->guid[0]), *(uint64_t *)(&part->guid[8])};

  // if partition type is zero, then the entry is not used
  if (type[0] == 0 && type[1] == 0)
    return;

  // if the first bit is set, then it's required by the firmware (we should not touch it)
  if (bit_get(part->attr, 1))
    return;

  gpt_debgf("loading partition %d", indx);
  gpt_debgf("|- Type: %g", type);
  gpt_debgf("|- GUID: %g", guid);
  gpt_debgf("|- Start LBA: %l", part->start_lba);
  gpt_debgf("|- End LBA: %l", part->end_lba);
  gpt_debgf("`- Attributes: 0x%p", part->attr);

  disk_part_t *dp = NULL;

  // add the new disk partition
  if (NULL == (dp = disk_part_add(disk, part->start_lba, (part->end_lba + 1) - part->start_lba))) {
    gpt_debg("failed to add a partition");
    return;
  }

  // load additional partition info and make the partition available
  dp->bootable  = bit_get(part->attr, 2);
  dp->available = true;
}

bool gpt_load(disk_t *disk) {
  struct gpt_table_header header;
  uint64_t                i = 0, e = 0, part_start = 0;

  bzero(&header, sizeof(header));

  if (!disk_do(disk, DISK_OP_READ, GPT_LBA, sizeof(header), (void *)&header)) {
    gpt_debg("failed to load the partition table header");
    return false;
  }

  if (header.signature != GPT_SIGNATURE) {
    gpt_debgf("bad signature (0x%x) for the partition table header", header.signature);
    return false;
  }

  gpt_debgf("GUID: 0x%p%p", ((uint64_t *)header.guid)[0], ((uint64_t *)header.guid)[1]);
  gpt_debgf("array LBA: %l", header.lba_array);
  gpt_debgf("array entry count: %d", header.entry_count);
  gpt_debgf("array entry size: %d", header.entry_size);

  if (disk->sector_size % header.entry_size) {
    gpt_debgf("sector size (%l) is not aligned by entry size (%l)", disk->sector_size, header.entry_size);
    return false;
  }

  typedef uint8_t gpt_part_entry_t[header.entry_size];

  uint64_t         entry_per_sector = div_floor(disk->sector_size, header.entry_size);
  gpt_part_entry_t entries[entry_per_sector];

  while (i < header.entry_count) {
    if (!disk_do(disk, DISK_OP_READ, header.lba_array + (i / entry_per_sector), disk->sector_size, (void *)entries)) {
      gpt_debgf("failed to read the partition entries %l-%l", i, i + entry_per_sector);
      goto next_entries;
    }

    for (e = 0; e < entry_per_sector; e++)
      __gpt_load_entry(disk, (void *)&entries[e], i + e);

  next_entries:
    i += entry_per_sector;
  }

  return true;
}
