#include "util/mem.h"

#include "fs/devfs.h"
#include "fs/fs.h"

#include "limits.h"
#include "types.h"
#include "errno.h"

int32_t devfs_new(fs_t *fs) {
  // setup filestystem operations
  bzero(&fs->ops, sizeof(fs->ops));
  fs->ops.free  = devfs_free;
  fs->ops.open  = devfs_open;
  fs->ops.close = devfs_close;
  fs->ops.read  = devfs_read;
  fs->ops.write = devfs_write;
  fs->ops.namei = devfs_namei;

  return 0;
}

int32_t devfs_open(struct fs *fs, fs_inode_t *inode) {
  if (inode->addr == 0)
    return 0;

  struct devfs_device *dev = devfs_device_at(inode->addr - 1);
  return NULL == dev ? -EIO : dev->ops->open(inode);
}

int32_t devfs_close(struct fs *fs, fs_inode_t *inode) {
  if (inode->addr == 0)
    return 0;

  struct devfs_device *dev = devfs_device_at(inode->addr - 1);
  return NULL == dev ? -EIO : dev->ops->close(inode);
}

int64_t __devfs_read_root(uint64_t offset, int64_t size, void *buffer) {
  // get the device at the given offset
  struct devfs_device *dev = devfs_device_at(offset);

  /*

   * check if we have a device at the given index otherwise
   * we have reached the end

  */
  if (NULL == dev)
    return 0;

  // make sure we don't copy too much
  if (size > NAME_MAX + 1)
    size = NAME_MAX + 1;

  // write the name to the buffer
  memcpy(buffer, (void *)dev->name, size);

  // return the read size
  return size;
}

int64_t devfs_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer) {
  // if we are working with the root directory, it's a directory read
  if (inode->addr == 0)
    return __devfs_read_root(offset, size, buffer);

  // otherwise call the read function for the given device
  struct devfs_device *dev = devfs_device_at(inode->addr - 1);
  return NULL == dev ? -EIO : dev->ops->read(inode, offset, size, buffer);
}

int64_t devfs_write(fs_t *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer) {
  if (inode->addr == 0)
    return -EISDIR;

  struct devfs_device *dev = devfs_device_at(inode->addr - 1);
  return NULL == dev ? -EIO : dev->ops->write(inode, offset, size, buffer);
}

int32_t devfs_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode) {
  if (NULL == fs || NULL == inode)
    return -EINVAL;

  // clear the data in the inode
  bzero(inode, sizeof(fs_inode_t));

  // setup the root directory inode
  if (NULL == dir) {
    inode->type   = FS_ENTRY_TYPE_DIR;
    inode->serial = fs_inode_serial(fs, inode);
    inode->mode   = MODE_USRR | MODE_USRE | MODE_GRPR | MODE_GRPE | MODE_OTHR | MODE_OTHE;
    return 0;
  }

  // only valid directory is the root directory
  if (dir->addr != 0)
    return -EINVAL;

  // call the namei for the given device
  struct devfs_device *dev   = NULL;
  uint64_t             index = 0;

  // try to find the given device
  if (NULL == (dev = devfs_device_find(name, &index)))
    return -ENOENT;

  // setup the inode
  inode->type   = FS_ENTRY_TYPE_FILE;
  inode->addr   = index + 1;
  inode->serial = fs_inode_serial(fs, inode);
  inode->mode   = dev->mode;

  return 0;
}

void devfs_free(fs_t *fs) {
  return;
}
