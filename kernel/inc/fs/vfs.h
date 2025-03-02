#pragma once
#ifndef __ASSEMBLY__

#include "fs/fs.h"
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
  fs_inode_t       inode;                    // inode for the node
  char             name[NAME_MAX + 1];       // name of the node
  uint64_t         ref_count;                // reference (pointer) counter
  struct vfs_node *parent, *child, *sibling; // parent, child and sibling pointers
} vfs_node_t;

extern vfs_node_t *vfs_root;              // root VFS node (/)
#define vfs_has_root() (NULL != vfs_root) // checks if we have a root node

// fs/vfs/node.c
#define vfs_node_is_directory(node) (node->inode.type == FS_ENTRY_TYPE_DIR)
#define vfs_node_is_fs_root(node)   (NULL == node->parent || node->fs != node->parent->fs)

#define vfs_node_open(node)  fs_open((node)->fs, &(node)->inode)
#define vfs_node_close(node) fs_close((node)->fs, &(node)->inode)

vfs_node_t *vfs_node_new(vfs_node_t *parent, char *name, fs_t *fs); // create (allocate) a VFS node
vfs_node_t *vfs_node_get(vfs_node_t *parent, char *name);           // get (find) a VFS node
int32_t     vfs_node_put(vfs_node_t *node);                         // put (free) a VFS node

// fs/vfs/vfs.c
int32_t vfs_open(vfs_node_t **node, char *path);                                   // open a path to obtain the VFS node
int32_t vfs_close(vfs_node_t *node);                                               // close (free) a VFS node
int64_t vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, void *buffer);  // read data from a node
int64_t vfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, void *buffer); // write to a node
int32_t vfs_mount(char *path, fs_t *fs);                                           // mount a filesystem to a path
int32_t vfs_umount(char *path);                                                    // unmount a filesystem from a path

#endif
