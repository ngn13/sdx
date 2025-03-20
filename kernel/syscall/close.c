#include "syscall.h"

#include "sched/sched.h"
#include "sched/task.h"

#include "types.h"
#include "errno.h"

int32_t sys_close(int32_t fd) {
  // obtain the file object at the given fd
  task_file_t *file = task_file_from(task_current, fd);
  int32_t      err  = 0;

  // check if the fd indexes to an acutal file object
  if (NULL == file)
    return -EBADF;

  // close & free the file
  if ((err = task_file_free(file, false)) != 0)
    return err;

  // update the last file descriptor
  if (fd == task_current->fd_last)
    task_current->fd_last--;

  // remove the file reference from the file list
  task_current->files[fd] = NULL;

  sys_debg("closed the file %d", fd);
  return 0;
}
