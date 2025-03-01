#pragma once
#include "fs/fs.h"
#include "types.h"

int32_t devfs_new(fs_t *fs);
int32_t devfs_namei(fs_t *fs, fs_inode_t *dir, char *name, fs_inode_t *inode);
int64_t devfs_read(fs_t *fs, fs_inode_t *inode, uint64_t offset, int64_t size, void *buffer);
void    devfs_free(fs_t *fs);
