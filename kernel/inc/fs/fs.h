#pragma once

#define FS_INIT_NAME "init" // "init" file name

#ifndef __ASSEMBLY__

#include "core/disk.h"
#include "util/timestamp.h"

/*

 * filesystem inode definitons

 * an inode stores information about an entry in the filesystem
 * an entry could be a file, directory, link etc. the inode is
 * obtained with the namei() filesystem call

*/
typedef enum {
  FS_ENTRY_TYPE_FILE = 0,
  FS_ENTRY_TYPE_LINK = 1,
  FS_ENTRY_TYPE_DIR  = 2,
} fs_entry_type_t;

#define MODE_SUID (04000)
#define MODE_GUID (02000)

#define MODE_USRR (0400)
#define MODE_USRW (0200)
#define MODE_USRE (0100)

#define MODE_GRPR (MODE_USRR >> 3)
#define MODE_GRPW (MODE_USRW >> 3)
#define MODE_GRPE (MODE_USRE >> 3)

#define MODE_OTHR (MODE_GRPR >> 3)
#define MODE_OTHW (MODE_GRPW >> 3)
#define MODE_OTHE (MODE_GRPE >> 3)

typedef struct {
  fs_entry_type_t type;   // entry type
  uint64_t        size;   // size of the entry
  uint64_t        addr;   // address of the entry
  uint64_t        serial; // file serial (unique)
  timestamp_t     ctime;  // creation time
  timestamp_t     atime;  // (last) access time
  timestamp_t     mtime;  // (last) modification time
  mode_t          mode;   // mode (permissions)
  // TODO: add mising fields such as UID, GID etc
} fs_inode_t;

#define fs_inode_serial(fs, inode) ((uint64_t)fs + (inode)->addr + (inode)->size)
#define fs_inode_compare(i1, i2)   ((i1)->serial == (i2)->serial)

/*

 * filesystem type definitions

 * filesystem structure stores information and functions specific
 * to the filesystem type, each filesystem may store data in a
 * different way, however these functions gives us a common way to
 * interact with the filesystems

*/
typedef enum {
  FS_TYPE_DETECT = 0,
  FS_TYPE_FAT32  = 1,
  FS_TYPE_DEVFS  = 2,
  // FS_TYPE_PROCFS = 3,
} fs_type_t;

#define FS_TYPE_DETECT_FIRST (FS_TYPE_FAT32) // first non-virtual filesystem
#define FS_TYPE_DETECT_LAST  (FS_TYPE_FAT32) // last non-virtual filesystem

typedef struct fs {
  fs_type_t          type;
  const disk_part_t *part;
  void              *data;

  struct {
    int32_t (*open)(struct fs *fs, fs_inode_t *inode);
    int32_t (*close)(struct fs *fs, fs_inode_t *inode);
    int64_t (*read)(struct fs *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
    int64_t (*write)(struct fs *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
    int32_t (*namei)(struct fs *fs, fs_inode_t *dir, char *name, fs_inode_t *inode);
    void (*free)(struct fs *fs);
  } ops;
} fs_t;

/*

 * filesystem functions & macros

 * these functions are used to create and interact with filesystems,
 * you'll see some of these are using the functions defined in the
 * filesystem structure, this provides an abstraction, where we can
 * just call fs_read() with the specific filesystem we are working
 * with and it will actually call the read function for that filesystem
 * we are working with

*/
fs_t *fs_new(fs_type_t type, disk_part_t *part); // creates a new filesystem using the specified type and the partition
const char *fs_name(fs_t *fs);                   // get filesystem type name
int32_t     fs_is_rootfs(fs_t *fs);              // checks provided fs is a valid root file system
int32_t     fs_default();                        // default filesystem function
#define fs_sector_size(fs) (NULL == fs->part ? 0 : fs->part->disk->sector_size)

// macros for calling fs functions defined in fs_t
int32_t fs_open(struct fs *fs, fs_inode_t *inode);
int32_t fs_close(struct fs *fs, fs_inode_t *inode);
int64_t fs_read(struct fs *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
int64_t fs_write(struct fs *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
int32_t fs_namei(struct fs *fs, fs_inode_t *dir, char *name, fs_inode_t *inode);
void    fs_free(struct fs *fs);

#endif
