#include "sched/sched.h"
#include "sched/task.h"

#include "types.h"
#include "errno.h"

int64_t sys_read(int32_t fd, void *buf, int64_t size) {
  task_file_t *file = task_file_from(task_current, fd);
  int64_t      ret  = 0;

  // check the file obtained with the fd
  if (NULL == file)
    return -EBADF;

  // TODO: check file->flags to make sure we can read

  // perform the read operation
  ret = vfs_read(file->node, file->offset, size, buf);

  if (ret > 0) {
    if (vfs_node_is_directory(file->node))
      // move onto the next directory entry
      file->offset++;
    else
      // increase the offset by read bytes
      file->offset += ret;
  }

  // return the result
  return ret;
}
