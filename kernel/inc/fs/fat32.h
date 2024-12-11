#pragma once
#include "fs/fs.h"
#include "types.h"
#include "util/printk.h"

/*

 * FAT32 filesystem driver definitions with no ur mom jokes (no cap no kizzy)
 * see fs/fat32 for the actual implementations

*/

#define fat32_fail(f, ...) pfail("FAT32: (0x%x) " f, fs->part, ##__VA_ARGS__)
#define fat32_debg(f, ...) pdebg("FAT32: (0x%x) " f, fs->part, ##__VA_ARGS__)

struct fat32_data {
  uint8_t  cluster_sector_count; // number of sectors per cluster
  uint16_t fat_sector;           // the starting sector of the FAT
  uint64_t first_data_sector;    // first sector that contains data clusters
  uint64_t root_cluster;         // the cluster number of the root directory
};

#define fat32_data() ((struct fat32_data *)fs->data)

#define fat32_data_sector_per_cluster() (fat32_data()->cluster_sector_count)
#define fat32_data_cluster_to_sector(cluster)                                                                          \
  (fat32_data()->first_data_sector + (fat32_data()->cluster_sector_count * (cluster - 2)))

#define fat32_data_bytes_per_cluster()       (fs_sector_size(fs) * fat32_data_sector_per_cluster())
#define fat32_data_cluster_to_bytes(cluster) (fat32_data_cluster_to_sector(cluster) * fs_sector_size(fs))

#define __fat32_read_raw(lba, sector_count, buf)                                                                       \
  (disk_read_raw(fs->part->disk, fs->part->start + lba, sector_count, (void *)buf))
#define __fat32_read_lba(lba, size, buf) (disk_read_lba(fs->part->disk, fs->part->start + lba, size, (void *)buf))
#define __fat32_read_cluster(cluster, buffer)                                                                          \
  __fat32_read_raw(fat32_data_cluster_to_sector(cluster), fat32_data_sector_per_cluster(), buffer)
#define __fat32_read(offset, size, buf)                                                                                \
  (disk_read(fs->part->disk, offset + (fs->part->start * fs->part->disk->sector_size), size, (void *)buf))

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
  uint32_t                  size;
} __attribute__((packed));

#define fat32_entry_cluster(entry)   ((uint32_t)((entry)->high_cluster_number << 16 | (entry)->low_cluster_number))
#define fat32_entry_is_last(entry)   (((uint8_t *)(entry))[0] == 0)
#define fat32_entry_is_unused(entry) (((uint8_t *)(entry))[0] == 0xe5)

#define fat32_entry_time_to_timestamp(date, time)                                                                      \
  timestamp_calc((date)->year + 1980,                                                                                  \
      (date)->month,                                                                                                   \
      (date)->day,                                                                                                     \
      NULL == time ? 0 : ((struct creation_time *)time)->hour,                                                         \
      NULL == time ? 0 : ((struct creation_time *)time)->minute,                                                       \
      NULL == time ? 0 : ((struct creation_time *)time)->second_half * 2)

// fs/fat32/entry.c
uint64_t fat32_cluster_next(fs_t *fs, uint64_t cluster);
int64_t  fat32_entry_get(fs_t *fs, uint64_t cluster, uint64_t offset, int64_t size, void *buffer);
int32_t  fat32_entry_from(fs_t *fs, uint64_t cluster, char *name, struct fat32_dir_entry *entry);

// fs/fat32/fat32.c
bool    fat32_new(fs_t *fs);
int32_t fat32_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode);
int64_t fat32_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
void    fat32_free(fs_t *fs);
