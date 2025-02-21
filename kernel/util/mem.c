#include "util/mem.h"
#include "util/printk.h"
#include "util/string.h"

#include "mm/heap.h"

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

char **charlist_copy(char *list[], uint64_t limit) {
  if (NULL == list)
    return NULL;

  uint64_t cur_size = 0, size = 0, count = 0;
  char   **cur = NULL, **copy = NULL;

  for (cur = list;; cur++) {
    if (++count > limit)
      return NULL;

    if (NULL == *cur)
      break;
  }

  copy  = heap_alloc(sizeof(char *) * count);
  count = 0;

  for (cur = list; *cur != NULL; cur++, count++) {
    size += cur_size = strlen(*cur) + 1;

    if (size > limit)
      return NULL;

    copy[count] = heap_alloc(cur_size);
    memcpy(copy[count], *cur, cur_size);
  }

  copy[count] = NULL;
  return copy;
}

void charlist_free(char **list) {
  if (NULL == list)
    return;

  for (char **cur = list; *cur != NULL; cur++)
    heap_free(*cur);

  heap_free(list);
}
