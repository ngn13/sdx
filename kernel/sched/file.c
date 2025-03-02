#include "sched/task.h"
#include "util/mem.h"

#include "config.h"
#include "types.h"
#include "errno.h"

int32_t task_file_fd_next(task_t *task) {
  int32_t fd = 0;

  // if we reached the last file descriptor, reset it
  if (task->fd_last >= CONFIG_TASK_FILES_MAX)
    task->fd_last = 0;

  // get the next file descriptor
  for (fd = task->fd_last; CONFIG_TASK_FILES_MAX > fd; fd++)
    if (NULL == task->files[fd])
      break;

  // if there's no available file descriptor, return an error
  if (fd >= CONFIG_TASK_FILES_MAX)
    return -EMFILE;

  return fd;
}

task_file_t *task_file_from(task_t *task, int32_t fd) {
  // check if the fd is valid
  if (fd > CONFIG_TASK_FILES_MAX || fd < 0)
    return NULL;

  // get the file at the given address
  return task->files[fd];
}

int32_t task_file_free(task_file_t *file, bool ignore_err) {
  int32_t err = 0;

  // call close on the VFS node
  if ((err = vfs_close(file->node)) != 0 && !ignore_err)
    return err;

  // free the file object
  heap_free(file);

  return err;
}

void task_file_clear(task_t *task) {
  // loop through all the files
  for (uint8_t i = 0; i < CONFIG_TASK_FILES_MAX; i++) {
    // skip files that are not in use
    if (NULL == task->files[i])
      continue;

    // close the file & free the object
    task_file_free(task->files[i], true);

    // remove the file from the list
    task->files[i] = NULL;
  }
}
