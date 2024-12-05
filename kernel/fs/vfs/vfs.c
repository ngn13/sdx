#include "fs/vfs.h"
#include "errno.h"

int32_t vfs_mount(vfs_node_t *node, fs_t *fs) {
  if (NULL == fs)
    return -EINVAL;

  vfs_node_t *new = vfs_node_new(fs);

  if (NULL == new)
    return -EFAULT;

  return vfs_node_add(node, new);
}

int32_t vfs_umount(vfs_node_t *node) {
  return vfs_node_del(node);
}

vfs_node_t *vfs_list(vfs_node_t *node, vfs_node_t *pos) {
  // TODO: implement
  return NULL;
}
