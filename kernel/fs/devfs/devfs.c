#include "fs/devfs.h"
#include "types.h"
#include "errno.h"

int32_t devfs_new(fs_t *fs) {
  return -ENOSYS;
}

int32_t devfs_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode) {
  return -ENOSYS;
}

int64_t devfs_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer) {
  return -ENOSYS;
}

void devfs_free(fs_t *fs) {
  return;
}
