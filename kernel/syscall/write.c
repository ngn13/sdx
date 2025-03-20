#include "sched/sched.h"
#include "sched/task.h"

#include "types.h"
#include "errno.h"

int64_t sys_write(int32_t fd, void *buf, uint64_t size) {
  task_file_t *file = task_file_from(task_current, fd);
  int64_t      ret  = 0;

  // check the file obtained with the fd
  if (NULL == file)
    return -EBADF;

  // TODO: check file->flags to make sure we can write

  // perform the read operation
  ret = vfs_write(file->node, file->offset, size, buf);

  // increase the offset by wroten bytes
  if (ret > 0)
    file->offset += ret;

  // return the result
  return ret;
}
