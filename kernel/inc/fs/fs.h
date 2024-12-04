#pragma once
#include "core/disk.h"
#include "fs/entry.h"

typedef void fs_data_t;

typedef enum {
  FS_TYPE_DETECT = 0,
  FS_TYPE_FAT32  = 1,
  //  FS_TYPE_PROCFS = 2,
  //  FS_TYE_DEVFS = 3,
} fs_type_t;

const char *fs_type_name(fs_type_t fs);

typedef struct fs {
  const fs_type_t    type;
  const disk_part_t *part;

  // filesystem specific stuff
  fs_data_t *data;
  fs_entry_t *(*list)(struct fs *fs, fs_entry_t *cur);
  void (*free)(struct fs *fs);
} fs_t;

#define fs_name(fs) fs_type_name(fs->type)
fs_t *fs_new(fs_type_t type, disk_part_t *part);
#define fs_list(fs, entry) fs->list(fs, fs->data, entry)
void fs_free(fs_t *fs);
