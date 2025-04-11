#pragma once
#include "util/printk.h"
#include "fs/fs.h"

#include "limits.h"
#include "types.h"

#define devfs_info(f, ...) pinfo("dev: " f, ##__VA_ARGS__)
#define devfs_fail(f, ...) pfail("dev: " f, ##__VA_ARGS__)
#define devfs_debg(f, ...) pdebg("dev: " f, ##__VA_ARGS__)

/*

 * describes the address for a single devfs device, and
 * contains the minor and the major number for the device

*/
typedef uint16_t devfs_addr_t;
#define devfs_addr(major, minor) ((devfs_addr_t)(((uint16_t)major << 8) | ((uint16_t)minor)))
#define devfs_major(addr)        (((devfs_addr_t)addr) >> 8)
#define devfs_minor(addr)        (((devfs_addr_t)addr) & UINT8_MAX)

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

// internal structures used to describe devices and device groups
struct devfs_device {
  const char           name[NAME_MAX + 1];
  devfs_addr_t         addr;
  mode_t               mode;
  struct devfs_device *next;
};

struct devfs_group {
  const char           name[NAME_MAX + 1];
  devfs_ops_t         *ops;
  struct devfs_device *head;
  struct devfs_group  *next;
};

// devfs/devfs.c
int32_t devfs_new(fs_t *fs);
void    devfs_free(fs_t *fs);
int32_t devfs_open(struct fs *fs, fs_inode_t *inode);
int32_t devfs_close(struct fs *fs, fs_inode_t *inode);
int64_t devfs_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer);
int64_t devfs_write(fs_t *fs, fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer);
int32_t devfs_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode);

// devfs/device.c
int32_t devfs_register(uint8_t major, const char *name, devfs_ops_t *ops);
int32_t devfs_unregister(uint8_t major);

int32_t devfs_create(devfs_addr_t addr, const char *name, mode_t mode);
int32_t devfs_destroy(devfs_addr_t addr);

struct devfs_group  *devfs_get_group(devfs_addr_t addr);
struct devfs_device *devfs_get_device(devfs_addr_t addr, char *name);
struct devfs_device *devfs_next_device(struct devfs_device *dev);
