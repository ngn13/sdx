#pragma once
#include "fs/vfs.h"
#include "types.h"

// stores information about the binary loaded into the memory
typedef struct {
  void    *addr, *entry;
  uint64_t pages;
} fmt_info_t;

// loaders for supported binary formats
int32_t elf_load(vfs_node_t *node, fmt_info_t *info);
// int32_t aout_load(vfs_node_t *node, void **addr);

int32_t fmt_load(vfs_node_t *node, fmt_info_t *info);
void    fmt_free(fmt_info_t *info);
