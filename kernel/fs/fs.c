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

struct fs_type_data {
  fs_type_t   type;
  const char *name;
  int32_t (*new)(fs_t *fs);
  bool supported;
};

struct fs_type_data fs_type_data[] = {
#ifdef CONFIG_FS_FAT32
    {FS_TYPE_FAT32, "FAT32", fat32_new, true},
#else
    {FS_TYPE_FAT32, "FAT32", NULL, false},
#endif
    {FS_TYPE_DEVFS, "DEVFS", devfs_new, true},
};

#define __fs_is_valid_type(type) (FS_TYPE_MAX >= type && type >= FS_TYPE_MIN)
#define __fs_type_name(type)     (__fs_is_valid_type(type) ? fs_type_data[type - 1].name : "unknown")
#define __fs_data(type)          (fs_type_data[type - 1])

int32_t __fs_new(fs_t *fs, uint8_t type) {
  int32_t err = 0;

  // check the filesystem type
  if (!__fs_is_valid_type(type)) {
    fs_fail("unknown filesystem type: %u", type);
    return -EINVAL;
  }

  // check if filesystem is supported
  if (!__fs_data(type).supported) {
    fs_fail("requested an unsupported filesystem: %s", __fs_type_name(type));
    return -ENODEV;
  }

  // attempt to create the requested filesystem
  if ((err = __fs_data(type).new(fs)) == 0)
    fs->type = type;

  // return the result
  return err;
}

int32_t fs_new(fs_t **fs, fs_type_t type, disk_part_t *part) {
  if (NULL == fs)
    return -EINVAL;
  ;

  int32_t err = 0;

  // allocate the new filesystem
  *fs = heap_alloc(sizeof(fs_t));

  // setup the new filesystem
  bzero(*fs, sizeof(fs_t));
  (*fs)->part = part;

  // check if we should manually detect the filesystem
  if (type != FS_TYPE_DETECT) {
    // if not, create the requested filesystem
    if ((err = __fs_new(*fs, type)) == 0)
      goto success;

    // if we fail, free the filesystem structure
    fs_fail("failed to create a %s filesystem: %s", __fs_type_name(type), strerror(err));
    goto fail;
  }

  // try to detect the filesystem
  for (type = FS_TYPE_DETECT_FIRST; type <= FS_TYPE_DETECT_LAST; type++)
    if (__fs_data(type).supported && (err = __fs_new(*fs, type)) == 0)
      goto success;

  /*

   * if we break out of the loop, this means we tried all the supported
   * filesystems, but all of them failed

  */
  fs_fail("no available filesystem for partition 0x%p", part);
  err = -EFAULT;

fail:
  heap_free(*fs);
  return err;

success:
  fs_debg("created a new filesystem");
  pdebg("    |- Filesystem: 0x%p", *fs);
  pdebg("    |- Partition: 0x%p", part);
  pdebg("    `- Type: %s", fs_name(*fs));
  return 0;
}

const char *fs_name(fs_t *fs) {
  return __fs_type_name(fs->type);
}

fs_type_t fs_type(const char *name) {
  fs_type_t type = FS_TYPE_MIN;

  for (; type <= FS_TYPE_MAX; type++)
    if (streq((char *)__fs_data(type).name, (char *)name))
      return type;

  return 0;
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
  fs_inode_t root, inode;

  // get the root directory inode
  if ((err = fs_namei(fs, NULL, NULL, &root)) != 0)
    return err;

  // attempt to get the inode of "init"
  if ((err = fs_namei(fs, &root, FS_INIT_NAME, &inode)) != 0)
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
