#pragma once
#include "fs/vfs.h"
#include "types.h"

int32_t fmt_load(vfs_node_t *node, void **entry, void **addr, uint64_t *count);

/*

 * loaders for supported executable formats
 * fmt_load tries each of these and tries to load node into the memory

*/
int32_t elf_load(vfs_node_t *node, void **entry, void **addr, uint64_t *count);
// int32_t aout_load(vfs_node_t *node, void **addr);
