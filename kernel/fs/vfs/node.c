#include "errno.h"
#include "fs/vfs.h"
#include "mm/vmm.h"

#include "util/mem.h"
#include "util/path.h"
#include "util/string.h"

vfs_node_t *vfs_node_list = NULL;

vfs_node_t *vfs_node_new(char *path) {
  if (NULL == path)
    return NULL;

  vfs_node_t *node = NULL, *cur = NULL;
  uint16_t    pi = 0;

  if (!path_is_absolute(path))
    return NULL;

  if (NULL == (node = vmm_alloc(sizeof(vfs_node_t))))
    return NULL;

  bzero(node, sizeof(vfs_node_t));

  for (; *path != 0; path++) {
    if (pi > PATH_MAX)
      return NULL;
    node->path[pi++] = *path;
  }

  if (NULL == (cur = vfs_node_list)) {
    vfs_node_list = node;
    return node;
  }

  while (NULL != cur->next)
    cur = cur->next;
  cur->next = node;

  return node;
}

void vfs_node_free(vfs_node_t *node) {
  if (NULL == node)
    return;
  vmm_free(node);
}
