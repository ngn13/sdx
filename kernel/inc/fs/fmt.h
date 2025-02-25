#pragma once
#include "mm/region.h"
#include "fs/vfs.h"
#include "types.h"

typedef struct {
  void     *entry;
  region_t *mem;
} fmt_t;

int32_t fmt_load(vfs_node_t *node, fmt_t *fmt);
void    fmt_free(fmt_t *fmt);

/*

 * loaders for supported executable formats
 * fmt_load tries each of these and tries to load node into the memory

*/
int32_t elf_load(vfs_node_t *node, fmt_t *fmt);
// int32_t aout_load(vfs_node_t *node, void **addr);
