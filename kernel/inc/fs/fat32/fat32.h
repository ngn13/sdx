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

#define fat32_read_disk(n, o, s, d)                                                                                    \
  (disk_do(node->vfs->part->disk, DISK_OP_READ, node->vfs->part->start + o, s, (void *)d))
#define fat32_cluster_to_sector(d, c) (d->first_data_sector + (d->cluster_sector_count * (c - 2)))
#define fat32_node_data()             ((fat32_data_t *)node->data)

#define fat32_debg(f)        pdebgf("FAT32: (0x%x) " f, node)
#define fat32_debgf(f, a...) pdebgf("FAT32: (0x%x) " f, node, a)

bool fat32_mount(vfs_node_t *node);
bool fat32_umount(vfs_node_t *node);
