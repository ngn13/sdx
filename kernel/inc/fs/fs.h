#pragma once
#include "core/disk.h"
#include "util/timestamp.h"

typedef enum {
  FS_ETYPE_FILE = 0,
  FS_ETYPE_LINK = 1,
  FS_ETYPE_DIR  = 2,
} fs_entry_type_t;

// describes an entry in the filesystem (file, dir, link etc.)
typedef struct {
  uint64_t parent; // parent directories address
  uint64_t index;  // index of the entry

  fs_entry_type_t type; // entry type
  uint64_t        size; // size of the entry
  uint64_t        addr; // address of the entry

  timestamp_t creation, access, mod; // times
} fs_entry_t;

#define FS_ROOT_INIT "init" // rootfs "init" name
typedef void fs_data_t;

typedef enum {
  FS_TYPE_DETECT = 0,
  FS_TYPE_FAT32  = 1,
  //  FS_TYPE_PROCFS = 2,
  //  FS_TYE_DEVFS = 3,
} fs_type_t;

const char *fs_type_name(fs_type_t fs);

typedef struct fs {
  fs_type_t          type;
  const disk_part_t *part;

  // filesystem specific stuff
  fs_data_t *data;

  /*

   * get an entry in the provided "dir" directory

   * "pre" is a pointer to the previous entry, if it's NULL we get the first entry
   * returns non-zero when no entries left

  */
  int32_t (*list)(struct fs *fs, fs_entry_t *dir, fs_entry_t *pre, fs_entry_t *cur);

  /*

   * get an entry's name

   * name is stored in the provided "name" buffer, if it's too small -EOVERFLOW
   * is returned, if the name is larger than NAME_MAX, -ENAMETOOLONG is returned
   * if it all goes well it returns the amount of chars written into the "name" buffer

   * also note that this function does NOT terminate the "name" buffer with the
   * NULL terminator '\0'

  */
  int32_t (*get)(struct fs *fs, fs_entry_t *ent, char *name, uint64_t size);
  void (*free)(struct fs *fs); // frees the filesystem data structures
} fs_t;

fs_t *fs_new(fs_type_t type,
    disk_part_t       *part); // creates a new filesystem using the specified type (and the partition if needed)
#define fs_name(fs)                 fs_type_name(fs->type)       // get filesystem type name
#define fs_list(fs, dir, pre, cur)  fs->list(fs, dir, pre, cur)  // macro for calling fs->list (see fs_t)
#define fs_get(fs, ent, name, size) fs->get(fs, ent, name, size) // macro for calling fs->get (see fs_t)
int32_t fs_is_rootfs(fs_t *fs);                                  // checks provided fs is a valid root file system
void    fs_free(fs_t *fs); // frees and cleans up the resources used by the given file system
