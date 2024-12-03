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
