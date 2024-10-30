#include "fs/fat32/fat32.h"
#include "mm/vmm.h"

struct fat32_lfn {
  uint8_t  order;
  uint16_t chars_first[5];
  uint8_t  attr; // always FAT32_ATTR_LFN
  uint8_t  type; // always zero
  uint8_t  checksum;
  uint16_t chars_mid[6];
  uint16_t first_cluster; // always zero
  uint16_t chars_last[2];
} __attribute__((packed));

#define fat32_lfn_is_last(l)  (bit_get(l->order, 7))
#define fat32_lfn_is_first(l) (bit_get(l->order, 1))

struct fat32_dir_entry {
  uint8_t name[11];
  uint8_t attr;
#define FAT32_ATTR_READONLY  0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0f
  uint8_t reserved;
  uint8_t creation_decisecond;
  struct creation_time {
    uint16_t hour        : 5;
    uint16_t minute      : 6;
    uint16_t second_half : 5;
  } __attribute__((packed)) creation_time;
  struct creation_date {
    uint16_t year  : 7;
    uint16_t month : 4;
    uint16_t day   : 5;
  } __attribute__((packed)) creation_date;
  struct creation_date      access_date;
  uint16_t                  high_cluster_number;
  struct creation_time      mod_time;
  struct creation_date      mod_date;
  uint16_t                  low_cluster_number;
#define fat32_dir_cluster(d) ((uint64_t)(d->high_cluster_number << 16 | d->low_cluster_number & UINT64_MAX))
  uint32_t size;
} __attribute__((packed));

char *__fat32_lfn_add_char(char *name, uint16_t *size, char c) {
  if (NULL == name)
    name = vmm_alloc(++*size);
  else
    name = vmm_realloc(name, ++*size);

  name[(*size) - 1] = c;
  return name;
}

char *__fat32_resolve_lfn(vfs_node_t *node, struct fat32_lfn *lfn, char *name, uint16_t *size, uint32_t depth) {
  if (lfn->attr != FAT32_ATTR_LFN)
    return name;

  uint8_t c = 0, i = 0;

  // parse the first name section
  for (i = 0; i < sizeof(lfn->chars_first); i++) {
    if ((c = (lfn->chars_first[i] >> 8) & 0xFF) == 0xFF || c == 0)
      goto complete;
    name = __fat32_lfn_add_char(name, size, c);
  }

  // the second name section
  for (i = 0; i < sizeof(lfn->chars_mid); i++) {
    if ((c = (lfn->chars_mid[i] >> 8) & 0xFF) == 0xFF || c == 0)
      goto complete;
    name = __fat32_lfn_add_char(name, size, c);
  }

  // and the last name section
  for (i = 0; i < sizeof(lfn->chars_last); i++) {
    if ((c = (lfn->chars_last[i] >> 8) & 0xFF) == 0xFF || c == 0)
      goto complete;
    name = __fat32_lfn_add_char(name, size, c);
  }

  if (!fat32_read_disk(node, fat32_node_data()->root_cluster_sector, sizeof(struct fat32_lfn) * depth, lfn)) {
    fat32_debg("failed to read the root directory");
    return false;
  }

complete:
  return name;
}

bool fat32_traverse(vfs_node_t *node, char *path) {
  struct fat32_dir_entry root;

  if (!fat32_read_disk(node, fat32_node_data()->root_cluster_sector, sizeof(root), &root)) {
    fat32_debg("failed to read the root directory");
    return false;
  }

  if (root.attr == FAT32_ATTR_LFN)

    fat32_debgf("first entry: %x %x %x %x %x...", root.name[0], root.name[1], root.name[2], root.name[3], root.name[4]);
  fat32_debgf("attr: 0x%x", root.attr);
  fat32_debgf("dec: %d", root.creation_decisecond);
  fat32_debgf("hour: %x", root.creation_time.hour);
  fat32_debgf("min: %x", root.creation_time.minute);
  fat32_debgf("sec: %x", root.creation_time.second_half);
}
