#include "fs/vfs.h"
#include "mm/vmm.h"

#include "util/mem.h"
#include "util/panic.h"
#include "util/printk.h"

#include "errno.h"

vfs_node_t *vfs_root = NULL;

vfs_node_t *vfs_node_new(fs_t *fs) {
  vfs_node_t *new_node = vmm_alloc(sizeof(vfs_node_t));

  if (NULL == new_node) {
    pfail("VFS: failed to allocate memory for a new node");
    return NULL;
  }

  bzero(new_node, sizeof(vfs_node_t));
  new_node->fs = fs;

  return new_node;
}

int32_t vfs_node_add(vfs_node_t *node, vfs_node_t *child) {
  if (NULL == child)
    return -EINVAL;

  if (NULL == node) {
    vfs_root = child;
    return 0;
  }

  vfs_node_t *trav = NULL;

  if (NULL == (trav = node->child)) {
    node->child = child;
    return 0;
  }

  while (trav->sibling != NULL)
    trav = trav->sibling;

  trav->sibling = child;
  trav->parent  = node;

  return 0;
}

int32_t vfs_node_del(vfs_node_t *node) {
  if (NULL == node)
    return -EINVAL;

  vfs_node_t *trav = node->child;

  while (NULL != trav) {
    vfs_node_del(trav);
    trav = trav->sibling;
  }

  if (NULL == node->parent) {
    vfs_node_free(node);
    vfs_root = NULL;
    return 0;
  }

  if (NULL == (trav = node->parent->child)) {
    // how did that happen?
    panic("Corrupted node tree (invalid parent)");
    return -EFAULT;
  }

  while (trav->sibling != node && trav->sibling != NULL)
    trav = trav->sibling;

  if (NULL == trav->sibling) {
    panic("Corrupted node tree (invalid sibling)");
    return -EFAULT;
  }

  trav->sibling = node->sibling;
  vfs_node_free(node);
  return 0;
}
