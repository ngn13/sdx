#include "fs/vfs.h"
#include "errno.h"

vfs_node_t *vfs_get(char *path) {
  if (NULL == path || *path == 0)
    return NULL;

  vfs_node_t *cur = NULL;
  char        name[NAME_MAX + 1];
  uint16_t    i = 0;

  switch (*path) {
  case '/': // absolute path? start from the root
    cur = vfs_root;
    path++;
    break;

  default: // relative path? start from the cwd
    // TODO: handle realitve path (cwd not implemented yet)
    return NULL;
  }

  while (cur != NULL && *path != 0) {
    vfs_path_next(path, name); // get next name from the path

    if (*name == 0 || strcmp(name, ".") == 0)
      continue; // skip empty names (double slash) or names that point to current name

    if (strcmp(name, "..") == 0) {
      cur = NULL == cur->parent ? cur : cur->parent; // check if we at root before moving to parent
      continue;
    }

    cur = vfs_node_get(cur, name);
  }

  if (NULL != cur)
    vfs_node_lock(cur);

  return cur;
}

int32_t vfs_mount(char *path, fs_t *fs) {
  if (NULL == path || NULL == fs)
    return -EINVAL;

  vfs_node_t *node = NULL;
  int32_t     err  = 0;

  // mount to the root
  if (strcmp(path, "/") == 0) {
    if (NULL == (node = vfs_node_new(NULL, fs))) {
      vfs_debg("failed to allocate the new root node");
      return -ENOMEM;
    }

    if ((err = vfs_node_add(NULL, node)) != 0) {
      vfs_debg("failed to mount node to root: %s", strerror(err));
      return err;
    }

    vfs_node_lock(node);
    vfs_info("mounted node to root");

    return 0;
  }

  // get the mount point
  if (NULL == (node = vfs_get(path))) {
    vfs_debg("mount point not found");
    return -ENOENT;
  }

  // you can only mount to a directory
  if (!vfs_node_is_directory(node)) {
    vfs_debg("mount point is not a directory");
    return -EINVAL;
  }

  // if everything is good, do the mount
  node->fs = fs;
  return 0;
}

int32_t vfs_umount(char *path) {
  vfs_node_t *node = NULL;

  if ((node = vfs_get(path)) == NULL)
    return -ENOENT;

  if (!vfs_node_is_fs_root(node))
    return -EINVAL;

  vfs_free(node);
  return 0;
}

int64_t vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, void *buffer) {
  if (NULL == node || NULL == buffer)
    return NULL;

  return fs_read(node->fs, vfs_node_is_fs_root(node) ? NULL : &node->inode, offset, size, buffer);
}

void vfs_free(vfs_node_t *node) {
  vfs_node_unlock(node);

  if (!vfs_node_is_locked(node))
    vfs_node_free(node);
}
