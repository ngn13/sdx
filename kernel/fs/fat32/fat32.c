#include "fs/fat32.h"
#include "fs/fs.h"
#include "fs/vfs.h"

#include "util/string.h"
#include "util/printk.h"
#include "util/mem.h"

#include "mm/heap.h"

#include "errno.h"

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

// (try to) load a FAT filesystem from the provided disk partition
int32_t fat32_new(fs_t *fs) {
  if (NULL == fs->part)
    return -EINVAL;

  struct fat32_boot_record fat_boot;
  struct fat32_fsinfo      fsinfo;

  if (!__fat32_read_lba(0, sizeof(fat_boot), &fat_boot)) {
    fat32_debg("failed to read the boot record");
    return -EFAULT;
  }

  if (!fat32_verify_extended(fat_boot.extended)) {
    fat32_debg("invalid boot record signature");
    return -EFAULT;
  }

  if (!__fat32_read_lba(fat_boot.extended.fsinfo_sector, sizeof(fsinfo), &fsinfo)) {
    fat32_debg("failed to read the fsinfo structure");
    return -EFAULT;
  }

  if (!fat32_verify_fsinfo(fsinfo)) {
    fat32_debg("failed to verify fsinfo signature");
    return -EFAULT;
  }

  if (NULL == (fs->data = heap_alloc(sizeof(struct fat32_data)))) {
    fat32_debg("failed to allocate node data");
    return -EFAULT;
  }

  bzero(fat32_data(), sizeof(struct fat32_data));

  fat32_data()->cluster_sector_count = fat_boot.cluster_sector_count;
  fat32_data()->fat_sector           = fat_boot.reserved_sector_count;
  fat32_data()->first_data_sector =
      fat_boot.reserved_sector_count + (fat_boot.fat_count * fat_boot.extended.fat_sector_count);
  fat32_data()->root_cluster = fat_boot.extended.root_cluster;

  fat32_debg("loaded FAT32 filesystem from 0x%p", fs->part);
  pdebg("       |- Boot record signature: 0x%x", fat_boot.extended.signature);
  pdebg("       |- FSInfo sector: %u", fat_boot.extended.fsinfo_sector);
  pdebg("       |- FSInfo signatures: %x,%x,%x", fsinfo.head_signature, fsinfo.body_signature, fsinfo.tail_signature);
  pdebg("       |- FSInfo free cluster count: %u", fsinfo.free_cluster_count);
  pdebg("       |- Table start sector: %u", fat32_data()->fat_sector);
  pdebg("       `- Root directory start cluster: %u", fat32_data()->root_cluster);

  // setup all the operations
  bzero(&fs->ops, sizeof(fs->ops));
  fs->ops.open  = fs_default;
  fs->ops.close = fs_default;
  fs->ops.read  = fat32_read;
  fs->ops.write = fat32_write;
  fs->ops.namei = fat32_namei;
  fs->ops.free  = fat32_free;

  return 0;
}

int64_t fat32_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  if (NULL == fs || NULL == buffer || size == 0)
    return -EINVAL;

  int64_t err = 0;

  switch (NULL == inode ? FS_ENTRY_TYPE_DIR : inode->type) {
  case FS_ENTRY_TYPE_DIR:
    if ((err = fat32_entry_get(fs, inode->addr, offset, size, buffer)) == -ERANGE)
      return 0; // we reached the end so we didn't read anything
    return err;

  case FS_ENTRY_TYPE_FILE:
    if (offset >= inode->size)
      return 0;

    uint64_t cluster_size = fat32_data_bytes_per_cluster(), cluster = inode->addr;
    uint64_t cur_size = 0;

    if ((cur_size = size) > inode->size - offset)
      cur_size = size = inode->size - offset;

    while (offset > cluster_size) {
      if ((cluster = fat32_cluster_next(fs, cluster)) == 0) {
        fat32_debg("failed to get the next cluster for inode: 0x%p", inode);
        return -EINVAL;
      }
      offset -= cluster_size;
    }

    while (cur_size > cluster_size) {
      if (!__fat32_read_raw(fat32_data_cluster_to_bytes(cluster) + offset, cluster_size, buffer)) {
        fat32_debg("failed to read the cluster %u for inode 0x%p", cluster, inode);
        return -EIO; // I/O error
      }

      if ((cluster = fat32_cluster_next(fs, cluster)) == 0) {
        fat32_debg("failed to get the next cluster for inode 0x%p", inode);
        return -EINVAL;
      }

      if (offset > 0)
        offset = 0; // we only use the offset once
      buffer += cluster_size;
      cur_size -= cluster_size;
    }

    if (cur_size > 0 && !__fat32_read(fat32_data_cluster_to_bytes(cluster) + offset, cur_size, buffer)) {
      fat32_debg("failed to read the cluster %u for inode 0x%p", cluster, inode);
      return -EIO;
    }

    return size;
  }

  return -EINVAL;
}

int64_t fat32_write(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  return -ENOSYS;
}

int32_t fat32_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode) {
  if (NULL == fs || NULL == inode)
    return -EINVAL;

  struct fat32_dir_entry entry;
  int32_t                err = 0;

  // clear the data in the inode
  bzero(inode, sizeof(fs_inode_t));

  // there is no entry for the root directory
  if (NULL == dir) {
    inode->type   = FS_ENTRY_TYPE_DIR;
    inode->addr   = fat32_data()->root_cluster;
    inode->serial = fs_inode_serial(fs, inode);

    // FAT32 doesn't support permissions, give full perms
    inode->mode =
        MODE_USRR | MODE_USRW | MODE_USRE | MODE_GRPR | MODE_GRPW | MODE_GRPE | MODE_OTHR | MODE_OTHW | MODE_OTHE;

    fat32_debg("obtained the root inode with serial %u", inode->serial);
    return 0;
  }

  // obtain the file entry, which contains all the data we need
  if ((err = fat32_entry_from(fs, dir->addr, name, &entry)) != 0) {
    fat32_debg("failed to obtain entry from name (\"%s\"): %s", name, strerror(err));
    return err;
  }

  // setup the inode information
  inode->type   = entry.attr & FAT32_ATTR_DIRECTORY ? FS_ENTRY_TYPE_DIR : FS_ENTRY_TYPE_FILE;
  inode->addr   = fat32_entry_cluster(&entry);
  inode->size   = entry.size;
  inode->serial = fs_inode_serial(fs, inode);

  inode->ctime = fat32_entry_time_to_timestamp(&entry.creation_date, &entry.creation_time);
  inode->mtime = fat32_entry_time_to_timestamp(&entry.mod_date, &entry.mod_time);
  inode->atime = fat32_entry_time_to_timestamp(&entry.access_date, NULL);
  inode->mode  = dir->mode;

  fat32_debg("obtained inode with serial %u for \"%s\"", inode->serial, name);
  return 0;
}

void fat32_free(fs_t *fs) {
  heap_free(fs->data);
}
