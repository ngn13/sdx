#include "sched/sched.h"
#include "sched/task.h"

#include "util/mem.h"
#include "errno.h"

int32_t sys_open(char *path, int32_t flags, mode_t mode) {
  vfs_node_t  *node = NULL;
  task_file_t *file = NULL;
  int32_t      fd   = -1;

  // try to obtain the node at the path
  if ((fd = vfs_open(&node, path)) != 0)
    goto end;

  // TODO: check permissions (mode)

  // get the next available file descriptor
  if ((fd = task_file_fd_next(task_current)) < 0)
    goto end;

  // update the last file descriptor
  if (fd > task_current->fd_last)
    task_current->fd_last = fd;

  // create a new file object
  if (NULL == (file = heap_alloc(sizeof(task_file_t)))) {
    fd = -ENOMEM;
    goto end;
  }

  // setup the file object
  bzero(file, sizeof(task_file_t));
  file->node  = node;
  file->flags = flags;

  // update the file pointer at the file descriptor index
  task_current->files[fd] = file;

end:
  if (fd < 0) {
    vfs_close(node);
    heap_free(file);
  }

  return fd;
}
