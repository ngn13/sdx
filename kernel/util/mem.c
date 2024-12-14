#include "util/mem.h"

bool bzero(void *data, int64_t size) {
  if (NULL == data || size <= 0)
    return false;

  for (--size; size >= 0; size--)
    ((char *)data)[size] = 0;

  return true;
}

void *memcpy(void *dst, void *src, int64_t size) {
  if (NULL == dst || NULL == src || size <= 0)
    return NULL;

  for (--size; size >= 0; size--, dst++, src++)
    *((char *)dst) = *((char *)src);

  return dst;
}

void memswap(char *x, char *y) {
  if (x == y)
    return;

  *x ^= *y;
  *y ^= *x;
  *x ^= *y;
}
