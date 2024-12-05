#pragma once
#include "fs/fs.h"
#include "types.h"
#include "util/printk.h"

struct fat32_data {
  uint8_t  cluster_sector_count; // number of sectors per cluster
  uint16_t fat_sector;           // the starting sector of the FAT
  uint64_t first_data_sector;    // first sector that contains data clusters
  uint64_t root_cluster_number;  // the cluster number of the root directory
  uint64_t root_cluster_sector;  // the starting sector of the root cluster
};

#define fat32_read_lba(o, s, d)       (disk_read_lba(fs->part->disk, fs->part->start + o, s, (void *)d))
#define fat32_cluster_to_sector(d, c) (d->first_data_sector + (d->cluster_sector_count * (c - 2)))
#define fat32_data()                  ((fat32_data *)fs->data)

#define fat32_fail(f, ...) pfail("FAT32: (0x%x) " f, fs->part, ##__VA_ARGS__)
#define fat32_debg(f, ...) pdebg("FAT32: (0x%x) " f, fs->part, ##__VA_ARGS__)

bool    fat32_new(fs_t *fs);
int32_t fat32_list(struct fs *fs, fs_entry_t *dir, fs_entry_t *last, fs_entry_t *cur);
int32_t fat32_get(fs_t *fs, fs_entry_t *ent, char *name, uint64_t size);
void    fat32_free(fs_t *fs);
