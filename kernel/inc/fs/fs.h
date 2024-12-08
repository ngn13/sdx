#pragma once
#include "core/disk.h"

#include "util/timestamp.h"
#include "util/string.h"

#include "limits.h"

#define FS_INIT_NAME "init" // "init" file name

/*

 * filesystem inode definitons

 * an inode stores information about an entry in the filesystem
 * such as a file, directory link etc.

*/
typedef enum {
  FS_ENTRY_TYPE_FILE = 0,
  FS_ENTRY_TYPE_LINK = 1,
  FS_ENTRY_TYPE_DIR  = 2,
} fs_entry_type_t;

typedef struct {
  fs_entry_type_t type;   // entry type
  uint64_t        serial; // file serial (unique)
  uint64_t        size;   // size of the entry
  uint64_t        addr;   // address of the entry
  timestamp_t     ctime;  // creation time
  timestamp_t     atime;  // (last) access time
  timestamp_t     mtime;  // (last) modification time

  // TODO: add mising fields such as UID, GID etc
} fs_inode_t;

#define fs_inode_serial(fs, inode) ((uint64_t)fs + (inode)->addr + (inode)->size)
#define fs_inode_compare(i1, i2)   ((i1)->serial == (i2)->serial)

/*

 * filesystem type definitions

 * used to store information and functions
 * specific to filesystem type

*/
typedef enum {
  FS_TYPE_DETECT = 0,
  FS_TYPE_FAT32  = 1,
  //  FS_TYPE_PROCFS = 2,
  //  FS_TYE_DEVFS = 3,
} fs_type_t;

typedef struct fs {
  fs_type_t          type;
  const disk_part_t *part;

  // filesystem specific stuff
  void *data;
  int32_t (*namei)(struct fs *fs, fs_inode_t *dir, char *name, fs_inode_t *inode); // get inode from a entry's name
  int64_t (*read)(struct fs *fs, fs_inode_t *inode, uint64_t offset, int64_t size,
      void *buffer);           // read a filesystem entry's content
  void (*free)(struct fs *fs); // free the filesystem data structures
} fs_t;

const char *fs_type_name(fs_type_t fs);

/*

 * filesystem functions

 * used to create and use different filesystems
 * includes macros for fs_t functions

*/
fs_t *fs_new(fs_type_t type,
    disk_part_t       *part); // creates a new filesystem using the specified type (and the partition if needed)
#define fs_sector_size(fs)             (NULL == fs->part ? 0 : fs->part->disk->sector_size)
#define fs_name(fs)                    fs_type_name(fs->type)          // get filesystem type name
#define fs_namei(fs, dir, name, inode) fs->namei(fs, dir, name, inode) // macro for calling fs->namei (see fs_t)
#define fs_read(fs, inode, offset, size, buffer)                                                                       \
  fs->read(fs, inode, offset, size, buffer) // macro for calling fs->read (see fs_t)
int32_t fs_is_rootfs(fs_t *fs);             // checks provided fs is a valid root file system
void    fs_free(fs_t *fs);                  // frees and cleans up the resources used by the given file system
