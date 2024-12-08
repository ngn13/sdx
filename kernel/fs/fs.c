#include "fs/fs.h"
#include "mm/vmm.h"

#include "util/printk.h"
#include "util/string.h"

#include "config.h"
#include "errno.h"

#define fs_debg(f, ...) pdebg("FS: (0x%x) " f, fs, ##__VA_ARGS__)

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
#include "fs/fat32.h"
    ret = fat32_new(new_fs);
#else
    ret = false;
#endif
    break;

  // unknown
  default:
    pfail("FS: no available filesystem found (Part: 0x%x)", part);
    vmm_free(new_fs);
    return NULL;
  }

  if (ret) {
    new_fs->type = type;
    pdebg("FS: (0x%x) created a new filesystem (Type: %s Part: 0x%x)", new_fs, fs_name(new_fs), new_fs->part);
    return new_fs;
  }

  if (!should_detect) {
    pfail("FS: failed to create a %s filesystem", fs_type_name(type));
    vmm_free(new_fs);
    return NULL;
  }

  type++;
  goto try_type;
}

int32_t fs_is_rootfs(fs_t *fs) {
  if (NULL == fs)
    return -EINVAL;

  /*

   * you might be wondering: what does it take to be a (possible) rootfs?

   * well, just having the "init" file in the root directory
   * "init" file's name and stuff defined in fs/fs.h
   * and this code just checks for that file

  */
  int32_t    err = 0;
  fs_inode_t inode;

  // attempt to get the inode of "init"
  if ((err = fs_namei(fs, NULL, FS_INIT_NAME, &inode)) != 0)
    return err;

  // is "init" a file?
  if (inode.type != FS_ENTRY_TYPE_FILE)
    return -EINVAL;

  // is "init" empty?
  if (inode.size == 0)
    return -EINVAL;

  return 0; // no? then we are good
}

void fs_free(fs_t *fs) {
  pdebg("FS: (0x%x) freeing a filesystem (Type: %s Part: 0x%x)", fs, fs_name(fs), fs->part);
  fs->free(fs);
  vmm_free(fs);
}
