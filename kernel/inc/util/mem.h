#pragma once
#include "types.h"

bool  bzero(void *data, int64_t size);
void *memcpy(void *dst, void *src, int64_t size);
void  memswap(char *x, char *y);

char **charlist_copy(char *list[], uint64_t limit);
void   charlist_free(char **list);
