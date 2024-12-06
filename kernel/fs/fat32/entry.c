#include "fs/fat32/fat32.h"

#include "util/bit.h"
#include "util/mem.h"

#include "errno.h"
#include "limits.h"

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

#define FAT32_CHARS_PER_LFN  13
#define fat32_lfn_order(l)   (l->order & 0b11111)
#define fat32_lfn_is_last(l) (bit_get(l->order, 6))

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
    uint8_t second_half : 5;
    uint8_t minute      : 6;
    uint8_t hour        : 5;
  } __attribute__((packed)) creation_time;
  struct creation_date {
    uint8_t day   : 5;
    uint8_t month : 4;
    uint8_t year  : 7;
  } __attribute__((packed)) creation_date;
  struct creation_date      access_date;
  uint16_t                  high_cluster_number;
  struct creation_time      mod_time;
  struct creation_date      mod_date;
  uint16_t                  low_cluster_number;
#define fat32_dir_cluster(d) ((uint64_t)(d->high_cluster_number << 16 | d->low_cluster_number & UINT64_MAX))
  uint32_t size;
} __attribute__((packed));

#define fat32_time_to_timestamp(date, time)                                                                            \
  timestamp_calc((date.year + 1980), date.month, date.day, time.hour, time.minute, (time.second_half * 2))
#define fat32_date_to_timestamp(date) timestamp_calc((date.year + 1980), date.month, date.day, 0, 0, 0)

int32_t fat32_entry_get(fs_t *fs, uint64_t cluster_sector, uint64_t _index, fs_entry_t *entry) {
  // https://wiki.osdev.org/FAT#Reading_Directories
  char                    cluster_buffer[fat32_data()->cluster_sector_count * fs->part->disk->sector_size];
  struct fat32_dir_entry *dirent = (void *)cluster_buffer;
  uint64_t                index  = _index;

  if (!fat32_read_raw(cluster_sector, fat32_data()->cluster_sector_count, cluster_buffer)) {
    fat32_debg("failed to read the directory entries at sector: %u", cluster_sector);
    return -EFAULT;
  }

  for (;; dirent++) {
    if (((uint8_t *)dirent)[0] == 0)
      return -EFAULT; // we reached the end

    if (((uint8_t *)dirent)[0] == 0xE5)
      continue; // skip unused entries

    // skip LFN entries
    if (dirent->attr == FAT32_ATTR_LFN)
      continue;

    if (index == 0)
      break;

    // skip till we reach the entry at the specified index
    index--;
  }

  bzero(entry, sizeof(fs_entry_t));

  entry->parent = cluster_sector;
  entry->index  = _index;

  entry->type = dirent->attr & FAT32_ATTR_DIRECTORY ? FS_ETYPE_DIR : FS_ETYPE_FILE;
  entry->size = dirent->size;

  entry->addr = 0;
  entry->addr |= dirent->low_cluster_number;
  entry->addr |= (dirent->high_cluster_number << 16);

  entry->creation = fat32_time_to_timestamp(dirent->creation_date, dirent->creation_time);
  entry->mod      = fat32_time_to_timestamp(dirent->mod_date, dirent->mod_time);
  entry->access   = fat32_date_to_timestamp(dirent->access_date);

  return 0;
}

// assumes the buffer is at least FAT32_CHARS_PER_LFN long
uint8_t __fat32_lfn_read(struct fat32_lfn *lfn, char *buf) {
  if (NULL == lfn || NULL == buf)
    return NULL;

  uint8_t i = 0, e = 0, c = 0;

  for (i = 0; i < sizeof(lfn->chars_first) && (c = lfn->chars_first[i] & 0x00ff) != 0 && c != 0xff; i++)
    buf[e++] = c;

  for (i = 0; i < sizeof(lfn->chars_mid) && (c = lfn->chars_mid[i] & 0x00ff) != 0 && c != 0xff; i++)
    buf[e++] = c;

  for (i = 0; i < sizeof(lfn->chars_last) && (c = lfn->chars_last[i] & 0x00ff) != 0 && c != 0xff; i++)
    buf[e++] = c;

  return e;
}

uint16_t __fat32_lfn_calc_size(struct fat32_lfn *lfn, uint8_t last_size) {
  // we need the last logical, first physical LFN entry to calculate the size
  if (!fat32_lfn_is_last(lfn))
    return 0;

  if (fat32_lfn_order(lfn) <= 0)
    return 0; // invalid LFN order

  uint64_t size = (fat32_lfn_order(lfn) - 1) * FAT32_CHARS_PER_LFN; // calculate the required size for next LFNs
  size += last_size;                                                // add current (last) LFNs size

  return size;
}

int32_t fat32_entry_name(fs_t *fs, fs_entry_t *entry, char *name, uint16_t name_size) {
  char cluster_buffer[fat32_data()->cluster_sector_count * fs->part->disk->sector_size],
      lfn_buffer[FAT32_CHARS_PER_LFN + 1];
  struct fat32_dir_entry *dirent   = NULL;
  uint16_t                lfn_size = 0, lfn_total = 0, name_index = 0;
  uint64_t                index = entry->index;

#define name_check(s)                                                                                                  \
  if (s > name_size)                                                                                                   \
    return -ERANGE;                                                                                                    \
  else if (s > NAME_MAX)                                                                                               \
  return -ENAMETOOLONG
#define name_append(c)                                                                                                 \
  do {                                                                                                                 \
    name_check(name_index + 1);                                                                                        \
    name[name_index++] = c;                                                                                            \
  } while (0);
#define name_copy(b, s)                                                                                                \
  do {                                                                                                                 \
    name_check(name_index + s);                                                                                        \
    memcpy(name + name_index, b, s);                                                                                   \
  } while (0);

  // just to prevent any possible memory issues
  lfn_buffer[FAT32_CHARS_PER_LFN] = 0;

  if (!fat32_read_raw(entry->parent, fat32_data()->cluster_sector_count, cluster_buffer)) {
    fat32_debg("failed to read the directory entries at sector: %u", entry->parent);
    return -EFAULT;
  }

next_entry:
  if (NULL == dirent)
    dirent = (void *)cluster_buffer;
  else
    dirent++;

  if (((uint8_t *)dirent)[0] == 0)
    return -EFAULT; // we reached the end

  if (((uint8_t *)dirent)[0] == 0xE5)
    return -EADDRNOTAVAIL; // specified entry is unused

  // skip LFN entries that doesn't belong to us
  if (dirent->attr == FAT32_ATTR_LFN && index != 0)
    goto next_entry;

  // skip till we reached the entry at the specified index
  if (index != 0) {
    index--;
    goto next_entry;
  }

  if (dirent->attr != FAT32_ATTR_LFN) {
    if (name_index != 0)
      return name_index;

    for (uint8_t i = 0; i < sizeof(dirent->name) && dirent->name[i] != 0x20; i++) {
      if (i == 8)
        name_append('.');
      name_append(dirent->name[i]);
    }

    return name_index;
  }

  // copy current LFN to the lfn_buffer
  lfn_size = __fat32_lfn_read((void *)dirent, lfn_buffer);

  // calculate the total size that all the LFNs are gonna take, will return non-zero on success
  if (lfn_total == 0) {
    lfn_total  = __fat32_lfn_calc_size((void *)dirent, lfn_size);
    name_index = lfn_total; // last entry first
  }

  // if we have the size calculated, copy the current LFN buffer to the name
  if (lfn_total != 0) {
    name_index -= lfn_size;
    name_copy(lfn_buffer, lfn_size);

    // we got all the LFNs, now lets just make sure name_index points to the end
    if (name_index == 0)
      name_index = lfn_total;
  }

  goto next_entry;
}
