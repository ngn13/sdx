#pragma once
#include "fs/disk.h"
#include "limits.h"
#include "types.h"

typedef enum vfs_type {
  VFS_TYPE_UNKNOWN = 0,
  VFS_TYPE_DISK    = 1,
  //  VFS_TYPE_DEVFS = 2,
  //  VFS_TYPE_PROCFS = 3,
} vfs_type_t;

typedef struct vfs {
  vfs_type_t   type;
  disk_part_t *part; // for VFS_TYPE_DISK
  struct vfs  *next;
} vfs_t;

int32_t vfs_mount(vfs_t *vfs, char *path); // mounts a VFS to a given path
int32_t vfs_umount(char *path);            // umount a VFS mounted at the given path

vfs_t  *vfs_register(vfs_type_t type, void *data); // creates a new VFS
int32_t vfs_unregister(vfs_t *vfs);                // removes a VFS

vfs_t *vfs_next(vfs_t *cur); // loops through the VFS list
vfs_t *vfs_detect();         // looks for a possible root VFS system

typedef struct vfs_node {
  vfs_t           *vfs;
  char             path[PATH_MAX + 1];
  struct vfs_node *next;

  // data stored by the file system
  void *data;

  // file system operations
  struct vfs_ops {
    bool (*umount)(struct vfs_node *node);
  } *ops;
} vfs_node_t;

typedef struct vfs_ops vfs_ops_t;

vfs_node_t *vfs_node_new(char *path);
void        vfs_node_free(vfs_node_t *node);
