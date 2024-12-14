#include "core/user.h"
#include "core/sched.h"

#include "fs/fmt.h"
#include "fs/vfs.h"

#include "mm/pm.h"

#include "types.h"
#include "errno.h"

int32_t user_exec(char *path) {
  vfs_node_t *node = vfs_get(path);
  fmt_info_t  finfo;
  int32_t     err = 0;

  // see if we found the node
  if (NULL == node)
    return -ENOENT;

  // we cannot execute a directory
  if (vfs_node_is_directory(node))
    return -EPERM;

  // TODO: handle shebang

  // try to load the file using a known format
  if ((err = fmt_load(node, &finfo)) < 0)
    return err;

  // make sure the allocated pages for the file are accessable by ring 3
  pm_set_all((uint64_t)finfo.addr, finfo.pages, PM_ENTRY_FLAG_US);
  return sched_new(node->name, finfo.entry, TASK_RING_USER);
  // return sched_new(node->name, finfo.entry, TASK_RING_KERNEL);
}
