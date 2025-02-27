#include "fs/fs.h"
#include "fs/vfs.h"

#include "util/mem.h"
#include "util/printk.h"

#include "mm/heap.h"
#include "errno.h"

vfs_node_t *vfs_root = NULL;
#define vfs_node_foreach(node) for (node = node->child; node != NULL; node = node->sibling)

vfs_node_t *vfs_node_new(char *name, fs_t *fs) {
  vfs_node_t *new_node  = heap_alloc(sizeof(vfs_node_t));
  uint64_t    name_size = NULL == name ? 0 : strlen(name);

  if (name_size > NAME_MAX)
    return NULL;

  if (NULL == new_node) {
    vfs_fail("failed to allocate memory for a new node");
    return NULL;
  }

  bzero(new_node, sizeof(vfs_node_t));
  memcpy(new_node->name, name, name_size);
  new_node->name[name_size] = 0;
  new_node->fs              = fs;

  vfs_debg("allocated a new node");
  pdebg("     |- Address: 0x%p", new_node);
  pdebg("     |- Filesystem: 0x%p (%s)", fs, fs_name(fs));
  pdebg("     `- Name: %s", new_node->name);

  return new_node;
}

int32_t vfs_node_add(vfs_node_t *node, vfs_node_t *child) {
  if (NULL == child)
    return -EINVAL;

  child->parent = node;

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
  return 0;
}

bool vfs_node_deleteable(vfs_node_t *node) {
  if (vfs_node_is_locked(node))
    return false;

  vfs_node_foreach(node) {
    if (!vfs_node_deleteable(node))
      return false;
  }

  return true;
}

int32_t vfs_node_free(vfs_node_t *node) {
  if (NULL == node)
    return -EINVAL;

  // is the node deleteable (is the node or childs locked)
  if (!vfs_node_deleteable(node))
    return -EINVAL;

  vfs_node_t *child = node->child, *pre = NULL;
  int32_t     err = 0;

  // free all the childs
  while (NULL != child) {
    pre   = child;
    child = child->sibling;

    if ((err = vfs_node_free(pre)) != 0)
      return err;
  }

  // remove the parent reference
  if (NULL == node->parent) {
    vfs_root = NULL;
    goto free;
  }

  for (pre = NULL, child = node->parent->child; child != NULL; pre = child, child = child->sibling) {
    if (child != node)
      continue;

    if (NULL == pre)
      node->parent->child = node->sibling;
    else
      pre->sibling = node->sibling;
  }

free:
  heap_free(node);
  return 0;
}

vfs_node_t *vfs_node_get(vfs_node_t *node, char *name) {
  if (NULL == name)
    return NULL;

  if (NULL == node && NULL == (node = vfs_root))
    return NULL;

  // see if we already have it as a child
  vfs_node_t *trav = node;
  vfs_node_foreach(trav) if (strcmp(trav->name, name) == 0) return trav;

  // no? lets see if we have it in the fs
  vfs_node_t *new = NULL;

  if ((new = vfs_node_new(name, node->fs)) == NULL)
    return NULL;

  if (fs_namei(node->fs, vfs_node_is_fs_root(node) ? NULL : &node->inode, name, &new->inode) != 0) {
    if (vfs_node_is_fs_root(node))
      vfs_debg("namei for \"%s\" failed on root node", name);
    else
      vfs_debg("namei for \"%s\" failed on non-root node", name);
    vfs_node_free(new);
    return NULL;
  }

  // if so lets add it as a child
  vfs_node_add(node, new);
  return new;
}
