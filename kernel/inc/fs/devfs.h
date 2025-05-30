#pragma once
#include "fs/fs.h"
#include "limits.h"
#include "types.h"

/*

 * describes the operations we can perform on a device
 * each device may have unique operations

*/
typedef struct {
  int32_t (*open)(fs_inode_t *inode);
  int32_t (*close)(fs_inode_t *inode);
  int64_t (*read)(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buf);
  int64_t (*write)(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buf);
} devfs_ops_t;

// describes a devfs device
struct devfs_device {
  const char           name[NAME_MAX + 1];
  devfs_ops_t         *ops;
  mode_t               mode;
  int32_t              addr;
  struct devfs_device *next;
};

// devfs/devfs.c
int32_t devfs_new(fs_t *fs);
void    devfs_free(fs_t *fs);
int32_t devfs_open(struct fs *fs, fs_inode_t *inode);
int32_t devfs_close(struct fs *fs, fs_inode_t *inode);
int64_t devfs_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer);
int64_t devfs_write(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer);
int32_t devfs_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode);

// devfs/devices.c
struct devfs_device *devfs_device_next(struct devfs_device *dev);
struct devfs_device *devfs_device_from_name(const char *name);
struct devfs_device *devfs_device_from_addr(int32_t addr);
int32_t              devfs_device_register(const char *name, devfs_ops_t *ops, mode_t mode);
int32_t              devfs_device_unregister(const char *name);
