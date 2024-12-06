#include "fs/fs.h"
#include "mm/vmm.h"

#include "util/printk.h"
#include "util/string.h"

#include "config.h"
#include "errno.h"

#define fs_debg(f, ...) pdebg("FS: (0x%x) " f, fs, ##__VA_ARGS__)

const char *fs_type_name(fs_type_t type) {
  switch (type) {
  case FS_TYPE_FAT32:
    return "FAT32";

  default:
    return "unknown";
  }
}

fs_t *fs_new(fs_type_t type, disk_part_t *part) {
  bool  ret = false, should_detect = false;
  fs_t *new_fs = vmm_alloc(sizeof(fs_t));
  new_fs->part = part;

  if ((should_detect = (type == FS_TYPE_DETECT)))
    type++;

try_type:
  switch (type) {
  // FAT32 filesystem
  case FS_TYPE_FAT32:
#ifdef CONFIG_FS_FAT32
#include "fs/fat32/fat32.h"
    ret = fat32_new(new_fs);
#else
    ret = false;
#endif
    break;

  // unknown
  default:
    pfail("FS: no available filesystem found (Part: 0x%x)", part);
    vmm_free(new_fs);
    return NULL;
  }

  if (ret) {
    new_fs->type = type;
    pdebg("FS: (0x%x) created a new filesystem (Type: %s Part: 0x%x)", new_fs, fs_name(new_fs), new_fs->part);
    return new_fs;
  }

  if (!should_detect) {
    pfail("FS: failed to create a %s filesystem", fs_type_name(type));
    vmm_free(new_fs);
    return NULL;
  }

  type++;
  goto try_type;
}

int32_t fs_is_rootfs(fs_t *fs) {
  if (NULL == fs)
    return -EINVAL;

  char       fname[strlen(FS_ROOT_INIT) + 1];
  int32_t    fname_size = 0, err = 0;
  fs_entry_t entry;

  if ((err = fs_list(fs, NULL, NULL, &entry)) != 0)
    return err;

  do {
    if (entry.type != FS_ETYPE_FILE)
      continue;

    if ((fname_size = fs_get(fs, &entry, fname, sizeof(fname) - 1)) < 0) {
      fs_debg("failed to get the name for entry %d: %s", entry.index, strerror(fname_size));
      continue;
    }

    fname[fname_size] = 0;

    if (strcmp(fname, FS_ROOT_INIT) == 0) {
      fs_debg("filesystem seems to be a valid root filesystem");
      return 0;
    }
  } while (fs_list(fs, NULL, &entry, &entry) == 0);

  return -ENOENT;
}

void fs_free(fs_t *fs) {
  pdebg("FS: (0x%x) freeing a filesystem (Type: %s Part: 0x%x)", fs, fs_name(fs), fs->part);
  fs->free(fs);
  vmm_free(fs);
}
