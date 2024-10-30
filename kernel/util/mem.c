#include "util/mem.h"

bool bzero(void *data, int64_t size) {
  if (NULL == data || size <= 0)
    return false;

  for (--size; size >= 0; size--)
    ((char *)data)[size] = 0;

  return true;
}

bool memcpy(void *dst, void *src, int64_t size) {
  if (NULL == dst || NULL == src || size <= 0)
    return false;

  for (--size; size >= 0; size--)
    ((char *)dst)[size] = ((char *)src)[size];

  return true;
}

void memswap(char *x, char *y) {
  if (x == y)
    return;

  *x ^= *y;
  *y ^= *x;
  *x ^= *y;
}
