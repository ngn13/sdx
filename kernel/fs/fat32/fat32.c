#include "fs/fat32/fat32.h"
#include "fs/disk.h"
#include "fs/vfs.h"

#include "util/bit.h"
#include "util/mem.h"
#include "util/printk.h"

#include "errno.h"
#include "mm/vmm.h"

/*

 * FAT32 filesystem operations
 * see vfs/vfs.h

*/
vfs_ops_t fat32_ops = {
    .umount = fat32_umount,
};

/*

 * FAT32 data structures
 * all of these are defined really well in https://wiki.osdev.org/FAT
 * and https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
 * so check those out if you want to follow through

*/
struct fat32_fsinfo {
  uint32_t head_signature;
#define FAT32_FSINFO_HEAD_SIG 0x41615252
  uint8_t  reserved0[480];
  uint32_t body_signature;
#define FAT32_FSINFO_BODY_SIG 0x61417272
  uint32_t free_cluster_count;      // cluster count, if all bits set, calculate yourself
  uint32_t available_cluster_start; // starting point to look for available clusters, if all bits set, start after root
                                    // cluster
  uint8_t  reserved1[12];
  uint32_t tail_signature;
#define FAT32_FSINFO_TAIL_SIG 0xAA550000
} __attribute__((packed));

struct fat32_extended_boot_record {
  uint32_t fat_sector_count; // number of sectors in FAT
  uint16_t flags;
  struct fat32_version {
    uint8_t major;
    uint8_t minor;
  } __attribute__((packed)) version;            // 2 byte major and minor version number
  uint32_t                  root_cluster;       // cluster number of the root directory
  uint16_t                  fsinfo_sector;      // sector number of fsinfo
  uint16_t                  backup_boot_sector; // sector number of the backup boot sector
  uint8_t                   reserved[12];       // should be zero
  uint8_t                   drive_number;       // https://en.wikipedia.org/wiki/INT_13H#List_of_INT_13h_services
  uint8_t                   nt_flags;           // windows NT flags;
  uint8_t                   signature;          // should be 0x28 or 0x29
  uint32_t                  volume_id;
  uint8_t                   volume_label[11]; // padded with spaces
  uint8_t                   system_id[8];     // "FAT32   "
} __attribute__((packed));

struct fat32_boot_record {
  uint8_t  bpb_jump[3];           // 3-byte instructions, used to jump over the BPB and EBPB
  uint64_t oem_id;                // OEM identifier
  uint16_t sector_size;           // sector size in bytes
  uint8_t  cluster_sector_count;  // how many sectors a cluster contains
  uint16_t reserved_sector_count; // how many reserved sectors are there
  uint8_t  fat_count;             // file allocation table (FAT) count
  uint16_t root_entry_count;      // number of root directory entries
  uint16_t sector_count_small;    // number of sectors, if set the zero, check sector_count_large instead
  uint8_t  media_desc;            // indicates media type
  uint16_t fat_sector_count;      // number of sectors in a FAT (only used by FAT12 and FAT16)
  uint16_t track_sector_count;    // sectors per track
  uint16_t head_count;            // head (or side) count
  uint32_t hidden_sector_count;   // number of hidden sectors
  uint32_t sector_count_large;    // number of sectors, used only if sector_count_small is not enough
  struct fat32_extended_boot_record extended;
} __attribute__((packed));

/*

 * macros for verifiying different data structures
 * again only used for initialization stuff

*/
#define fat32_verify_extended(e) (e.signature == 0x29 || e.signature == 0x28)
#define fat32_verify_fsinfo(i)                                                                                         \
  (i.head_signature == FAT32_FSINFO_HEAD_SIG && i.body_signature == FAT32_FSINFO_BODY_SIG &&                           \
      i.tail_signature == FAT32_FSINFO_TAIL_SIG)

/*

 * (try to) mount the FAT32 file system to the given VFS node
 * returns zero on success

*/
bool fat32_mount(vfs_node_t *node) {
  struct fat32_boot_record fat_boot;

  if (!fat32_read_disk(node, 0, sizeof(fat_boot), &fat_boot)) {
    fat32_debg("failed to read the boot record");
    return false;
  }

  fat32_debgf("boot record signature: 0x%x", fat_boot.extended.signature);
  fat32_debgf("boot record fsinfo sector: %d", fat_boot.extended.fsinfo_sector);

  if (!fat32_verify_extended(fat_boot.extended)) {
    fat32_debg("invalid boot record signature");
    return false;
  }

  struct fat32_fsinfo fsinfo;

  if (!fat32_read_disk(node, fat_boot.extended.fsinfo_sector, sizeof(fsinfo), &fsinfo)) {
    fat32_debg("failed to read the fsinfo structure");
    return false;
  }

  fat32_debgf(
      "fsinfo signatures: 0x%x, 0x%x, 0x%x", fsinfo.head_signature, fsinfo.body_signature, fsinfo.tail_signature);
  fat32_debgf("fsinfo free cluster count: %d", fsinfo.free_cluster_count);

  if (!fat32_verify_fsinfo(fsinfo)) {
    fat32_debg("failed to verify fsinfo signature");
    return false;
  }

  struct fat32_data *data = NULL;

  if (NULL == (data = node->data = vmm_alloc(sizeof(struct fat32_data)))) {
    fat32_debg("failed to allocate node data");
    return false;
  }

  bzero(node->data, sizeof(struct fat32_data));

  data->cluster_sector_count = fat_boot.cluster_sector_count;
  data->fat_sector           = fat_boot.reserved_sector_count;
  data->first_data_sector = fat_boot.reserved_sector_count + (fat_boot.fat_count * fat_boot.extended.fat_sector_count);
  data->root_cluster_number = fat_boot.extended.root_cluster;
  data->root_cluster_sector = fat32_cluster_to_sector(data, data->root_cluster_number);

  fat32_debgf("table start sector: %l", data->fat_sector);
  fat32_debgf("root directory start sector: %l", data->root_cluster_sector);

  node->ops = &fat32_ops;

  return true;
fail:
  vmm_free(node->data);
  node->data = NULL;
  return false;
}

/*

 * unmount a FAT32 filesystem from a given VFS node
 * returns zero on success

*/
bool fat32_umount(vfs_node_t *node) {
  vmm_free(node->data);
  return true;
}