#include "util/string.h"
#include "util/mem.h"
#include "fs/vfs.h"

#include "types.h"
#include "errno.h"

#define __vfs_path_next(path, name)                                                                                    \
  do {                                                                                                                 \
    for (i = 0; *path != 0; i++, path++) {                                                                             \
      if (i > NAME_MAX)                                                                                                \
        return NULL;                                                                                                   \
                                                                                                                       \
      if (*path == '/') {                                                                                              \
        path++;                                                                                                        \
        break;                                                                                                         \
      }                                                                                                                \
                                                                                                                       \
      name[i] = *path;                                                                                                 \
    }                                                                                                                  \
    name[i] = 0;                                                                                                       \
  } while (0);

vfs_node_t *__vfs_find(char *path) {
  if (NULL == path || *path == 0)
    return NULL;

  vfs_node_t *cur = NULL, *parent = NULL;
  char        name[NAME_MAX + 1];
  uint16_t    i = 0;

  switch (*path) {
  // absolute path? start from the root
  case '/':
    cur = vfs_node_get(NULL, NULL);
    path++;
    break;

  // relative path? start from the cwd
  default:
    // TODO: handle realitve path (cwd not implemented yet)
    return NULL;
  }

  while (cur != NULL && *path != 0) {
    // get the next name from the path
    __vfs_path_next(path, name); // get next name from the path

    // skip empty names ("//") or names that point to current name ("/./")
    if (*name == 0 || strcmp(name, ".") == 0)
      continue;

    // check if we are at root ("/") before moving up ("..")
    if (strcmp(name, "..") == 0) {
      cur = NULL == cur->parent ? cur : cur->parent;
      continue;
    }

    // get the next name, and free the parent
    cur = vfs_node_get(parent = cur, name);
    vfs_node_put(parent);
  }

  // return the found node
  return cur;
}

int32_t vfs_open(vfs_node_t **node, char *path) {
  if (NULL == node || NULL == path || *path == 0)
    return -EINVAL;

  int32_t err = 0;

  // find the node from the path
  if ((*node = __vfs_find(path)) == NULL)
    return -ENOENT;

  // call the filesystem's open call
  if ((err = vfs_node_open(*node)) != 0) {
    vfs_debg("failed to open the VFS node @ 0x%p: %s", *node, strerror(err));
    vfs_node_put(*node);
    return err;
  }

  return 0;
}

int32_t vfs_close(vfs_node_t *node) {
  if (NULL == node)
    return -EINVAL;

  int32_t err = 0;

  // call the filesystem's close call
  if ((err = vfs_node_close(node)) != 0) {
    vfs_debg("failed to close the VFS node @ 0x%p: %s", node, strerror(err));
    return err;
  }

  // free the VFS node
  if ((err = vfs_node_put(node)) != 0 && err != -EBUSY)
    return err;

  return 0;
}

int64_t vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, void *buffer) {
  if (NULL == node || NULL == buffer)
    return NULL;
  return fs_read(node->fs, &node->inode, offset, size, buffer);
}

int64_t vfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, void *buffer) {
  if (NULL == node || NULL == buffer)
    return NULL;
  return fs_write(node->fs, &node->inode, offset, size, buffer);
}

int32_t vfs_mount(char *path, fs_t *fs) {
  if (NULL == path || NULL == fs)
    return -EINVAL;

  vfs_node_t *node = NULL;
  int32_t     err  = 0;

  // mount to the root
  if (strcmp(path, "/") == 0) {
    if (NULL == (node = vfs_node_new(NULL, NULL, fs))) {
      vfs_debg("failed to allocate the new root node");
      return -ENOMEM;
    }

    /*

     * increase the reference counter of the node

     * otherwise next time vfs_close() is called on the node
     * node may be removed, which would remove our mount point
     * by increasing the reference counter we make sure the
     * node won't be removed until vfs_unmount() is called

    */
    node->ref_count++;
  }

  else {
    // get the mount point
    if ((node = __vfs_find(path)) == NULL) {
      vfs_debg("failed to get the mount point");
      return -ENOENT;
    }

    // you can only mount to a directory
    if (!vfs_node_is_directory(node)) {
      vfs_debg("mount point is not a directory");
      return -EINVAL;
    }
  }

  // first save the inode we are mounting on
  memcpy(&node->inode, &node->mount, sizeof(node->inode));

  // then obtain the new inode for the filesystem
  if ((err = fs_namei(fs, NULL, NULL, &node->inode)) != 0) {
    vfs_debg("failed to obtain the root inode");
    vfs_node_put(node);
    return err;
  }

  // switch to the new filesystem for the mount
  node->mount_fs = node->fs;
  node->fs       = fs;

  vfs_info("mounted node 0x%p to %s", node, path);
  return 0;
}

int32_t vfs_umount(char *path) {
  if (NULL == path)
    return -EINVAL;

  vfs_node_t *node = NULL;

  // get the VFS node at the given path
  if (NULL == (node = __vfs_find(path))) {
    vfs_debg("failed to get the mount point");
    return -ENOENT;
  }

  // check if VFS node is a mount point
  if (!vfs_node_is_fs_root(node) || !vfs_node_is_mountpoint(node)) {
    vfs_debg("node 0x%p is not a vaild mount point", node);
    return -EINVAL;
  }

  // copy back the old mount point's inode and fs
  memcpy(&node->mount, &node->inode, sizeof(node->mount));
  node->fs = node->mount_fs;

  // clear the mount point data
  bzero(&node->mount, sizeof(node->mount));
  node->mount_fs = NULL;

  vfs_node_put(node);        // __vfs_find()
  return vfs_node_put(node); // vfs_mount()
}

fs_t *vfs_fs(char *path) {
  if (NULL == path)
    return NULL;

  // get the node at the given path
  vfs_node_t *node = __vfs_find(path);

  // check if the node actually exists
  if (NULL == node)
    return NULL;

  // return the filesystem of the node
  vfs_node_put(node); // __vfs_find()
  return node->fs;
}
