#pragma once
#include "fs/vfs.h"
#include "types.h"
#include "util/printk.h"

typedef struct fat32_data {
  uint8_t  cluster_sector_count; // number of sectors per cluster
  uint16_t fat_sector;           // the starting sector of the FAT
  uint64_t first_data_sector;    // first sector that contains data clusters
  uint64_t root_cluster_number;  // the cluster number of the root directory
  uint64_t root_cluster_sector;  // the starting sector of the root cluster
} fat32_data_t;

#define fat32_read_disk(o, s, d)      (disk_do(vfs_part(vfs)->disk, DISK_OP_READ, vfs_part(vfs)->start + o, s, (void *)d))
#define fat32_cluster_to_sector(d, c) (d->first_data_sector + (d->cluster_sector_count * (c - 2)))
#define fat32_debg(f, ...)            pdebg("FAT32: (0x%x) " f, vfs, ##__VA_ARGS__)
#define fat32_data()                  ((fat32_data_t *)vfs->fs_data)

bool         fat32_load(vfs_t *vfs);
bool         fat32_unload(vfs_t *vfs);
vfs_entry_t *fat32_list(vfs_t *vfs, vfs_entry_t *target, vfs_entry_t *cur);
char        *fat32_get(vfs_t *vfs, vfs_entry_t *target);

extern vfs_fs_t fat32_fs;
