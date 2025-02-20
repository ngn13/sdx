#include "fs/fs.h"
#include "mm/heap.h"

#include "types.h"
#include "util/printk.h"
#include "util/string.h"

#include "config.h"
#include "errno.h"

#define fs_debg(f, ...) pdebg("FS: " f, ##__VA_ARGS__)
#define fs_info(f, ...) pinfo("FS: " f, ##__VA_ARGS__)
#define fs_fail(f, ...) pfail("FS: " f, ##__VA_ARGS__)

const char *fs_type_name(fs_type_t type) {
  switch (type) {
  case FS_TYPE_FAT32:
    return "FAT32";

  default:
    return "unknown";
  }
}

fs_t *fs_new(fs_type_t type, disk_part_t *part) {
  fs_t   *new_fs        = heap_alloc(sizeof(fs_t));
  bool    should_detect = false;
  int32_t err           = 0;

  new_fs->part = part;

  if ((should_detect = (type == FS_TYPE_DETECT)))
    type++;

try_type:
  switch (type) {
  // FAT32 filesystem
  case FS_TYPE_FAT32:
#ifdef CONFIG_FS_FAT32
#include "fs/fat32.h"
    err = fat32_new(new_fs);
#else
    err = -ENOSYS;
#endif
    break;

  // unknown
  default:
    fs_fail("no available filesystem for partition 0x%p", part);
    heap_free(new_fs);
    return NULL;
  }

  if (err == 0) {
    new_fs->type = type;
    fs_debg("created a new filesystem");
    pdebg("    |- Filesystem: 0x%p", new_fs);
    pdebg("    |- Partition: 0x%p", new_fs->part);
    pdebg("    `- Type: %s", fs_name(new_fs));
    return new_fs;
  }

  if (!should_detect) {
    fs_fail("failed to create a %s filesystem: %s", fs_type_name(type), strerror(err));
    heap_free(new_fs);
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

   * "init" file's name and stuff defined in fs/fs.h and this code just
   * checks for that file

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
  fs_debg("freeing filesystem 0x%p", fs);
  fs->free(fs);
  heap_free(fs);
}
