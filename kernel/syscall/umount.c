#include "syscall.h"
#include "fs/vfs.h"

#include "util/string.h"

#include "errno.h"
#include "types.h"

int32_t sys_umount(char *target) {
  int32_t err = 0;
  fs_t   *fs  = NULL;

  if (NULL == target)
    return -EINVAL;

  // obtain the filesystem at the target node
  if (NULL == (fs = vfs_fs(target))) {
    sys_debg("failed to get the fs at %s", target, strerror(err));
    return -EINVAL;
  }

  // umount the target node
  if ((err = vfs_umount(target)) != 0) {
    sys_debg("failed to umount %s: %s", target, strerror(err));
    return err;
  }

  // free the filesystem (we no longer need it)
  fs_free(fs);
  return 0;
}
