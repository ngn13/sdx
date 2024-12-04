#pragma once
#include "fs/fs.h"
#include "limits.h"
#include "types.h"

// defines a entry in the VFS (file, directory etc.)
typedef struct vfs_node {
  char        name[NAME_MAX];
  fs_entry_t *entry;
  fs_t        fs;

  struct vfs_node *parent;
  struct vfs_node *next;
} vfs_node_t;
