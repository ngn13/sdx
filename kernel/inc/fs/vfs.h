#pragma once
#include "fs/fs.h"
#include "mm/vmm.h"

#include "limits.h"
#include "types.h"

// defines a entry in the VFS (file, directory etc.)
typedef struct vfs_node {
  char       name[NAME_MAX];
  fs_entry_t entry;
  fs_t      *fs;

  struct vfs_node *parent, *child;
  struct vfs_node *sibling;
} vfs_node_t;

extern vfs_node_t *vfs_root;

// fs/vfs/node.c
vfs_node_t *vfs_node_new(fs_t *fs);
#define vfs_node_free(node) vmm_free(node)
int32_t vfs_node_add(vfs_node_t *node, vfs_node_t *child);
int32_t vfs_node_del(vfs_node_t *node);

// fs/vfs/vfs.c
int32_t     vfs_mount(vfs_node_t *node, fs_t *fs); // mounts FS to the given node, if node is NULL it mounts it to root
int32_t     vfs_umount(vfs_node_t *node);          // unmounts a given node
vfs_node_t *vfs_list(vfs_node_t *node, vfs_node_t *pos); // gets nodes attached to a given node
