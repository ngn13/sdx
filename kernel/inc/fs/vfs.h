#pragma once
#include "util/lock.h"
#include "mm/vmm.h"
#include "fs/fs.h"

#include "limits.h"
#include "types.h"

#define vfs_debg(f, ...) pdebg("VFS: (0x%x:0x%x) " f, node, NULL == node ? 0 : node->fs, ##__VA_ARGS__)
#define vfs_info(f, ...) pinfo("VFS: (0x%x:0x%x) " f, node, NULL == node ? 0 : node->fs, ##__VA_ARGS__)
#define vfs_fail(f, ...) pfail("VFS: (0x%x:0x%x) " f, node, NULL == node ? 0 : node->fs, ##__VA_ARGS__)

// defines a entry in the VFS (file, directory etc.)
typedef struct vfs_node {
  fs_t            *fs;                 // filesystem the inode belongs to
  char             name[NAME_MAX + 1]; // name of the node
  fs_inode_t       inode;              // inode for the node
  spinlock_t       use_lock;           // is node in use?
  struct vfs_node *parent, *child, *sibling;
} vfs_node_t;

#define vfs_node_is_locked(node) spinlock_is_locked(node->use_lock)
#define vfs_node_unlock(node)    spinlock_unlock(node->use_lock)
#define vfs_node_lock(node)      spinlock_lock(node->use_lock)

#define vfs_path_next(path, name)                                                                                      \
  do {                                                                                                                 \
    for (i = 0; *path != '/' && *path != 0; i++, path++) {                                                             \
      if (i > NAME_MAX)                                                                                                \
        return NULL;                                                                                                   \
      name[i] = *path;                                                                                                 \
    }                                                                                                                  \
    name[i] = 0;                                                                                                     \
  } while (0);

// fs/vfs/node.c
extern vfs_node_t *vfs_root;
vfs_node_t        *vfs_node_new(char *name, fs_t *fs);
vfs_node_t        *vfs_node_get(vfs_node_t *node, char *name);
int32_t            vfs_node_add(vfs_node_t *node, vfs_node_t *child);
int32_t            vfs_node_free(vfs_node_t *node);
#define vfs_node_is_directory(node) (node->inode.type == FS_ENTRY_TYPE_DIR)
#define vfs_node_is_fs_root(node)   (NULL == node->parent || node->fs != node->parent->fs)

// fs/vfs/vfs.c
#define vfs_has_root() (NULL != vfs_root)
vfs_node_t *vfs_get(char *_path);
int64_t     vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, void *buffer);
int32_t     vfs_mount(char *path, fs_t *fs);
int32_t     vfs_umount(char *path);
void        vfs_free(vfs_node_t *node);
