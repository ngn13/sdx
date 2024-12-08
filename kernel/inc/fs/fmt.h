#pragma once
#include "fs/vfs.h"
#include "types.h"

// loaders for supported binary formats
int32_t elf_load(vfs_node_t *node, void **addr);
// int32_t aout_load(vfs_node_t *node, void **addr);

int32_t fmt_load(vfs_node_t *node, void *addr);
