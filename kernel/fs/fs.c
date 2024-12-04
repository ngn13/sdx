#include "fs/fs.h"
#include "config.h"
#include "mm/vmm.h"
#include "util/printk.h"

const char *fs_type_name(fs_type_t type) {
  switch (type) {
  case FS_TYPE_FAT32:
    return "FAT32";

  default:
    return "unknown";
  }
}

fs_t *fs_new(fs_type_t type, disk_part_t *part) {
  bool  ret = false, should_detect = false;
  fs_t *new_fs = vmm_alloc(sizeof(fs_t));
  new_fs->part = part;

  if ((should_detect = (type == FS_TYPE_DETECT)))
    type++;

try_type:
  switch (type) {
  // FAT32 filesystem
  case FS_TYPE_FAT32:
#ifdef CONFIG_FS_FAT32
#include "fs/fat32/fat32.h"
    ret = fat32_new(new_fs);
#else
    ret = false;
#endif
    break;

  // unknown
  default:
    pfail("FS: no available filesystem found");
    vmm_free(new_fs);
    return NULL;
  }

  if (ret)
    return new_fs;

  if (!should_detect) {
    pfail("failed to create a %s filesystem", fs_type_name(type));
    vmm_free(new_fs);
    return NULL;
  }

  type++;
  goto try_type;
}

void fs_free(fs_t *fs) {
  fs->free(fs);
  vmm_free(fs);
}
