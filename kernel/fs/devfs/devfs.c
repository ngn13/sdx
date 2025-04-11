#include "util/mem.h"

#include "fs/devfs.h"
#include "fs/fs.h"

#include "limits.h"
#include "types.h"
#include "errno.h"

int32_t devfs_new(fs_t *fs) {
  // clear the filesystem operations
  bzero(&fs->ops, sizeof(fs->ops));

  // setup filestystem operations
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

  struct devfs_group *group = devfs_get_group(inode->addr);
  return NULL == group ? -EIO : group->ops->open(inode);
}

int32_t devfs_close(struct fs *fs, fs_inode_t *inode) {
  if (inode->addr == 0)
    return 0;

  struct devfs_group *group = devfs_get_group(inode->addr);
  return NULL == group ? -EIO : group->ops->close(inode);
}

int64_t devfs_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  struct devfs_device *dev = NULL;

  // if we are working with the root directory, it's a directory read
  if (inode->addr == 0) {
    // get the device at the given offset
    while (NULL != (dev = devfs_next_device(dev)) && offset > 0)
      offset--;

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

  // otherwise call the read function for the given device
  struct devfs_group *group = devfs_get_group(inode->addr);
  return NULL == group ? -EIO : group->ops->read(inode, offset, size, buffer);
}

int64_t devfs_write(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  if (inode->addr == 0)
    return -EISDIR;

  struct devfs_group *group = devfs_get_group(inode->addr);
  return NULL == group ? -EIO : group->ops->write(inode, offset, size, buffer);
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

  struct devfs_device *dev = NULL;

  // try to find the given device
  if (NULL == (dev = devfs_get_device(0, name)))
    return -ENOENT;

  // setup the inode
  inode->type   = FS_ENTRY_TYPE_FILE;
  inode->addr   = dev->addr;
  inode->serial = fs_inode_serial(fs, inode);
  inode->mode   = dev->mode;

  return 0;
}

void devfs_free(fs_t *fs) {
  return;
}
