#include "syscall.h"
#include "fs/fs.h"

#include "util/string.h"

#include "types.h"
#include "errno.h"

int32_t sys_mount(char *source, char *target, char *filesystem, int32_t flags) {
  disk_part_t *part = NULL;
  fs_type_t    type = 0;
  fs_t        *fs   = NULL;
  int32_t      err  = 0;

  /*

   * user should specify a target, and if no source is specified
   * a filesystem should be specified

  */
  if (NULL == target || (NULL == source && NULL == filesystem))
    return -EINVAL;

  if (NULL == filesystem)
    type = FS_TYPE_DETECT;

  else {
    if ((type = fs_type(filesystem)) == FS_TYPE_INVALID)
      return -ENODEV;
  }

  // TODO: obtain the partition information from source path

  if ((err = fs_new(&fs, type, NULL)) != 0) {
    sys_debg("failed to create the fs to mount: %s", strerror(err));
    return err;
  }

  if ((err = vfs_mount(target, fs)) != 0) {
    sys_debg("failed to bind the created filesystem to %s: %s", target, strerror(err));
    fs_free(fs);
    return err;
  }

  return 0;
}
