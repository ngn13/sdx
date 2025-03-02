#pragma once
#include "fs/fs.h"
#include "types.h"

/*

 * describes the operations we can perform on a device
 * each device may have unique operations

*/
typedef struct {
  int32_t (*open)(fs_inode_t *inode);
  int32_t (*close)(fs_inode_t *inode);
  int64_t (*read)(fs_inode_t *inode, uint64_t offset, uint64_t size, char *buf);
  int64_t (*write)(fs_inode_t *inode, uint64_t offset, uint64_t size, char *buf);
} devfs_ops_t;

// describes a devfs device
struct devfs_device {
  const char          *name;
  devfs_ops_t         *ops;
  struct devfs_device *next;
};

// devfs/devfs.c
int32_t devfs_new(fs_t *fs);
void    devfs_free(fs_t *fs);
int32_t devfs_open(struct fs *fs, fs_inode_t *inode);
int32_t devfs_close(struct fs *fs, fs_inode_t *inode);
int64_t devfs_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
int64_t devfs_write(fs_t *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
int32_t devfs_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode);

// devfs/devices.c
struct devfs_device *devfs_device_next(struct devfs_device *dev);
struct devfs_device *devfs_device_find(const char *name);
int32_t              devfs_device_register(const char *name, devfs_ops_t *ops);
int32_t              devfs_device_unregister(const char *name);
