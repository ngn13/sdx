#include "util/printk.h"
#include "util/string.h"
#include "util/mem.h"

#include "fs/fs.h"
#include "fs/fat32.h"
#include "fs/devfs.h"

#include "mm/heap.h"

#include "config.h"
#include "types.h"
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

int32_t __fs_new(fs_t *fs, fs_type_t type) {
  int32_t err = 0;

  switch (type) {
  // FAT32 filesystem
  case FS_TYPE_FAT32:
#ifdef CONFIG_FS_FAT32
    err = fat32_new(fs);
#else
    err = -ENOSYS;
#endif
    break;

  // dev filesystem
  case FS_TYPE_DEVFS:
    err = devfs_new(fs);

  // unknown
  default:
    fs_fail("unknown filesystem type: %u", type);
    return -EFAULT;
  }

  if (!err)
    fs->type = type;
  return err;
}

fs_t *fs_new(fs_type_t type, disk_part_t *part) {
  fs_t   *new_fs = heap_alloc(sizeof(fs_t));
  int32_t err    = 0;

  // setup the new filesystem
  bzero(new_fs, sizeof(fs_t));
  new_fs->part = part;

  // check if we should manually detect the filesystem
  if (type != FS_TYPE_DETECT) {
    // if not, create the requested filesystem
    if ((err = __fs_new(new_fs, type)) == 0)
      goto success;

    // if we fail, free the filesystem structure
    fs_fail("failed to create a %s filesystem: %s", fs_type_name(type), strerror(err));
    goto fail;
  }

  // try to detect the filesystem
  for (uint8_t i = FS_TYPE_DETECT_FIRST; i <= FS_TYPE_DETECT_LAST; i++)
    if ((err = __fs_new(new_fs, i)) == 0)
      goto success;

  fs_fail("no available filesystem for partition 0x%p", part);
fail:
  heap_free(new_fs);
  return NULL;

success:
  fs_debg("created a new filesystem");
  pdebg("    |- Filesystem: 0x%p", new_fs);
  pdebg("    |- Partition: 0x%p", new_fs->part);
  pdebg("    `- Type: %s", fs_name(new_fs));
  return new_fs;
}

const char *fs_name(fs_t *fs) {
  return fs_type_name(fs->type);
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

int32_t fs_default() {
  return 0;
}

int32_t fs_open(struct fs *fs, fs_inode_t *inode) {
  if (NULL != fs->ops.open)
    return fs->ops.open(fs, inode);
  return -ENOSYS;
}

int32_t fs_close(struct fs *fs, fs_inode_t *inode) {
  if (NULL != fs->ops.close)
    return fs->ops.close(fs, inode);
  return -ENOSYS;
}

int64_t fs_read(struct fs *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer) {
  if (NULL != fs->ops.read)
    return fs->ops.read(fs, inode, offset, size, buffer);
  return -ENOSYS;
}

int64_t fs_write(struct fs *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer) {
  if (NULL != fs->ops.write)
    return fs->ops.write(fs, inode, offset, size, buffer);
  return -ENOSYS;
}

int32_t fs_namei(struct fs *fs, fs_inode_t *dir, char *name, fs_inode_t *inode) {
  if (NULL != fs->ops.namei)
    return fs->ops.namei(fs, dir, name, inode);
  return -ENOSYS;
}

void fs_free(fs_t *fs) {
  fs_debg("freeing filesystem 0x%p", fs);
  fs->ops.free(fs);
  heap_free(fs);
}
