#include "fs/fs.h"
#include "fs/vfs.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/mem.h"

#include "mm/heap.h"
#include "errno.h"

vfs_node_t *vfs_root = NULL;

#define __vfs_node_foreach_child(node) for (node = node->child; node != NULL; node = node->sibling)
#define __vfs_node_free(node)          heap_free(node)

bool __vfs_node_deleteable(vfs_node_t *node) {
  // check the reference counter for the node
  if (node->ref_count != 0)
    return false;

  // check if the child nodes are deletable
  __vfs_node_foreach_child(node) {
    if (!__vfs_node_deleteable(node))
      return false;
  }

  return true;
}

vfs_node_t *vfs_node_new(vfs_node_t *parent, char *name, fs_t *fs) {
  // check the arguments
  if (NULL == fs)
    return NULL;

  uint64_t    name_size = NULL == name ? 0 : strlen(name);
  vfs_node_t *node      = NULL;
  fs_inode_t  inode;

  // check the VFS node name size
  if (name_size > NAME_MAX)
    return NULL;

  // try to obtain the inode for the name
  if (NULL != parent && fs_namei(parent->fs, vfs_node_is_fs_root(parent) ? NULL : &parent->inode, name, &inode) != 0) {
    vfs_debg("namei for \"%s\" failed on node 0x%p", name, parent);
    return NULL;
  }

  // create a new VFS node for the name
  if (NULL == (node = heap_alloc(sizeof(vfs_node_t)))) {
    vfs_fail("failed to allocate memory for a new node");
    return NULL;
  }

  // setup VFS node's contents
  bzero(node, sizeof(vfs_node_t));
  memcpy(&node->inode, &inode, sizeof(fs_inode_t));
  memcpy(node->name, name, name_size);
  node->name[name_size] = 0;
  node->fs              = fs;

  vfs_debg("adding a node to the VFS tree");
  pdebg("     |- Address: 0x%p (%s)", node, node->name);
  if (NULL != parent)
    pdebg("     |- Parent: 0x%p (%s)", parent, parent->name);
  pdebg("     `- Filesystem: 0x%p (%s)", node->fs, fs_name(node->fs));

  // increase the reference counter for the node
  node->ref_count++;

  // check if the node is the root
  if (NULL == (node->parent = parent))
    return vfs_root = node;

  // otherwise attach node to parent
  node->sibling        = parent->child;
  return parent->child = node;
}

vfs_node_t *vfs_node_get(vfs_node_t *parent, char *name) {
  if (NULL == name)
    return NULL;

  if (NULL == parent && NULL == (parent = vfs_root))
    return NULL;

  vfs_node_t *cur = parent;

  // see if we already have it as a child
  __vfs_node_foreach_child(cur) {
    if (streq(cur->name, name)) {
      cur->ref_count++;
      return cur;
    }
  }

  /*

   * if there is no child node with the given name,
   * create a new node for the child

  */
  return vfs_node_new(parent, name, parent->fs);
}

int32_t vfs_node_put(vfs_node_t *node) {
  if (NULL == node)
    return -EINVAL;

  vfs_node_t *trav = NULL, *prev = NULL;
  int32_t     err = 0;

  // decrease the reference counter for the node
  if (node->ref_count != 0)
    node->ref_count--;

  // check if we can delete the node
  if (!__vfs_node_deleteable(node))
    return -EBUSY;

  vfs_debg("deleting a node from the VFS tree");
  pdebg("     |- Address: 0x%p (%s)", node, node->name);
  if (NULL != node->parent)
    pdebg("     |- Parent: 0x%p (%s)", node->parent, node->parent->name);
  pdebg("     `- Filesystem: 0x%p (%s)", node->fs, fs_name(node->fs));

  // if so, first delete all the childs
  for (trav = node->child; NULL != trav;) {
    prev = trav;
    trav = trav->sibling;

    if ((err = vfs_node_put(trav)) != 0)
      return err;
  }

  /*

   * remove the parent's reference to the node, there are two
   * cases here, the node maybe the root node, in this case we
   * just remove the root node's reference

   * or the parent maybe a non-root node, in this case we go through
   * parent's child node list and remove ourselves from the list

  */
  if (NULL == node->parent) {
    vfs_root = NULL;
    goto free_node;
  }

  /*

   * or the parent maybe a non-root node, in this case we go through
   * parent's child node list and remove ourselves from the list

  */
  for (trav = node->parent->child; NULL != trav; prev = trav, trav = trav->sibling) {
    // continue through the loop till we find ourselves
    if (trav != node)
      continue;

    if (NULL == prev)
      // if we are the first child, remove the parent's reference
      node->parent->child = trav->sibling;
    else
      // otherwise remove the previous siblings reference
      prev->sibling = trav->sibling;
  }

free_node:
  __vfs_node_free(node);
  return 0;
}
