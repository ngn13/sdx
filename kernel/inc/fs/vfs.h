#pragma once
#include "core/disk.h"
#include "types.h"

// defines a entry in the VFS filesystem (file, directory etc.)
typedef struct vfs_entry {
  // TODO: add other data to find the entry (lba, offset, size and stuff)
  struct vfs_entry *next;
} vfs_entry_t;

// defines the type of the VFS
typedef uint8_t vfs_type_t;
#define VFS_TYPE_UNKNOWN (0b1)
#define VFS_TYPE_DISK    (0b10)
// #define VFS_TYPE_DEVFS (0b100)
// #define VFS_TYPE_PROCFS (0b1000)

// defines a filesystem for the VFS
typedef struct {
  const char *name;  // FS name
  vfs_type_t  types; // compatible VFS types

  // FS functions
  bool (*load)(void *vfs);                                                // attempt to load FS from VFS
  void (*unload)(void *vfs);                                              // unload FS from VFS
  char *(*get)(void *vfs, vfs_entry_t *target);                           // get an entry's name
  vfs_entry_t *(*list)(void *vfs, vfs_entry_t *target, vfs_entry_t *cur); // get entries in a target
} vfs_fs_t;

typedef struct vfs {
  struct vfs  *next;
  vfs_entry_t *head;

  // VFS type specific data
  vfs_type_t type;
  void      *type_data;
#define vfs_part(v) ((disk_part_t *)v->type_data)

  // VFS filesystem specific data
  vfs_fs_t *fs;
  void     *fs_data;
} vfs_t;

#define vfs_available(v)  (NULL != v->fs)
#define vfs_unload(v, t)  vfs->fs->unload(v, t, c)
#define vfs_get(v, t)     vfs->fs->get(v, t, c)
#define vfs_list(v, t, c) vfs->fs->list(v, t, c)

vfs_t  *vfs_register(vfs_type_t type, void *type_data); // creates a new VFS
int32_t vfs_unregister(vfs_t *vfs);                     // removes a VFS
vfs_t  *vfs_next(vfs_t *cur);                           // loops through the VFS list
