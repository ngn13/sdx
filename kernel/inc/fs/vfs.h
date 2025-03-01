#pragma once
#include "fs/fs.h"
#include "util/lock.h"

#include "limits.h"
#include "types.h"

#define vfs_debg(f, ...) pdebg("VFS: " f, ##__VA_ARGS__)
#define vfs_info(f, ...) pinfo("VFS: " f, ##__VA_ARGS__)
#define vfs_fail(f, ...) pfail("VFS: " f, ##__VA_ARGS__)

/*

 * VFS consists of directories and files (nodes) on different
 * filesystems connected to each other with mount points, creating
 * the root filesystem tree

 * to traverse through the tree and to modify it easily, each VFS
 * node has 3 pointers:
 * - parent: points to the parent node, so the parent directory
 * - child: points to the first child node
 * - sibling: points to the next child of the parent

 * here is an example root filesystem tree structure, showcasing
 * how the node pointers are used:

 *   .---------------------[/]
 *   |                      |
 * child                  parent
 *   |______________________|______________________.
 *   |                      |                      |
 *   v                      |                      |
 * [init] --- sibling --> [boot] --- sibling --> [etc]

*/

typedef struct vfs_node {
  fs_t            *fs;                       // filesystem the inode belongs to
  char             name[NAME_MAX + 1];       // name of the node
  fs_inode_t       inode;                    // inode for the node
  uint64_t         ref_count;                // reference (pointer) counter
  struct vfs_node *parent, *child, *sibling; // parent, child and sibling pointers
} vfs_node_t;

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
