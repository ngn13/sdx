#include "mm/heap.h"
#include "mm/vmm.h"

#include "util/printk.h"
#include "util/panic.h"
#include "util/math.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

#define heap_fail(f, ...) pfail("Heap: " f, ##__VA_ARGS__)
#define heap_warn(f, ...) pwarn("Heap: " f, ##__VA_ARGS__)
#define heap_debg(f, ...) pdebg("Heap: " f, ##__VA_ARGS__)

#define HEAP_CHUNK_MAGIC     0xa71e394b53a81759
#define HEAP_CHUNK_DATA_SIZE (16)
#define HEAP_CHUNK_META_SIZE (sizeof(struct heap_chunk) - HEAP_CHUNK_DATA_SIZE)
#define HEAP_CHUNK_PER_PAGE  (VMM_PAGE_SIZE / sizeof(struct heap_chunk))

/*

 * free memory is split into chunks, each chunk consists of these
 * two fields:
 * - meta (16 bytes): stores metadata information about the chunk
 * - data (16 bytes): actual data stored in the chunk

 * when the chunk is free (not used) "meta" stores the next and previous
 * chunk's address (so it points to the next and the previous chunk), which
 * creates a doubly linked list

 * when memory is allocated, we combine these small chunks together and remove
 * them from this doubly linked list of chunks, so when the chunk is used (not
 * free) "meta" stores a magic value that's used to verify the allocated chunk
 * when later it's feed to heap_free, and it also stores the combined chunk's
 * size

 * also thompson loot lama huge chungus amongus

*/

struct heap_chunk {
  uint64_t meta[2];                    // metadata information about the current chunk
  char     data[HEAP_CHUNK_DATA_SIZE]; // data in the chunk
};

#define __heap_chunk_meta_next(chunk)           ((struct heap_chunk *)chunk->meta[0])
#define __heap_chunk_meta_next_set(chunk, next) (chunk->meta[0] = (uint64_t)(next))

#define __heap_chunk_meta_prev(chunk)           ((struct heap_chunk *)chunk->meta[1])
#define __heap_chunk_meta_prev_set(chunk, prev) (chunk->meta[1] = (uint64_t)(prev))

#define __heap_chunk_meta_size(chunk)           (chunk->meta[1])
#define __heap_chunk_meta_size_set(chunk, size) (chunk->meta[1] = (uint64_t)(size))

#define __heap_chunk_is_magical(chunk) (chunk->meta[0] == HEAP_CHUNK_MAGIC)
#define __heap_chunk_data_clear(chunk) (bzero((chunk)->data, HEAP_CHUNK_DATA_SIZE))

struct heap_chunk *heap_chunk_first = NULL;
struct heap_chunk *heap_chunk_last  = NULL;

int32_t __heap_extend() {
  struct heap_chunk *cur = vmm_map(1, VMM_FLAGS_DEFAULT);

  if (NULL == cur) {
    heap_fail("failed to allocate a new page for extending the heap");
    return -EFAULT;
  }

  if (NULL == heap_chunk_first)
    heap_chunk_first = cur;

  if (NULL != heap_chunk_last)
    __heap_chunk_meta_next_set(heap_chunk_last, cur);

  __heap_chunk_meta_prev_set(cur, heap_chunk_last);

  for (uint8_t i = 0; i < HEAP_CHUNK_PER_PAGE; i++, cur++) {
    __heap_chunk_data_clear(cur);
    if (i != 0)
      __heap_chunk_meta_prev_set(cur, cur - 1);
    __heap_chunk_meta_next_set(cur, cur + 1);
  }

  __heap_chunk_meta_next_set((heap_chunk_last = cur - 1), NULL);

  return 0;
}

struct heap_chunk *__heap_chunk_next(struct heap_chunk *cur) {
  if (NULL == heap_chunk_first || NULL == heap_chunk_last)
    __heap_extend();

  if (NULL == cur)
    return heap_chunk_first;

  if (NULL == __heap_chunk_meta_next(cur))
    __heap_extend();

  return __heap_chunk_meta_next(cur);
}

void *heap_alloc(uint64_t size) {
  struct heap_chunk *cur = NULL, *start = NULL, *end = NULL;
  uint64_t           total_size = 0;

  for (cur = __heap_chunk_next(cur); NULL != cur && total_size < size; end = cur, cur = __heap_chunk_next(cur)) {
    // first chunk? we can only use the "data" part of the chunk for storing data
    if (NULL == start) {
      total_size += HEAP_CHUNK_DATA_SIZE;
      start = cur;
      continue;
    }

    /*

     * make sure the chunk is contiguous with the previous one
     * if not, we'll need a new starting point, we can't fit the
     * allocation here

    */
    if (end + 1 != cur) {
      total_size = 0;
      start      = NULL;
      continue;
    }

    // not the first chunk, so we can use the entire chunk for storing data
    total_size += sizeof(struct heap_chunk);
  }

  if (NULL == cur || size > total_size) {
    heap_fail("%u byte allocation failed", size);
    return NULL;
  }

  /*

   * this removes the previous chunk's reference to start chunk
   * so this is basically: start->prev->next = end->next

  */
  if ((cur = __heap_chunk_meta_prev(start)) != NULL)
    __heap_chunk_meta_next_set(cur, __heap_chunk_meta_next(end));

  /*

   * this removes the next chunk's reference to end chunk
   * so this is basically: end->next->prev = start->prev

  */
  if ((cur = __heap_chunk_meta_next(end)) != NULL)
    __heap_chunk_meta_prev_set(cur, __heap_chunk_meta_prev(start));

  // if start is the first chunk, set first chunk to end->next
  if (heap_chunk_first == start)
    heap_chunk_first = __heap_chunk_meta_next(end);

  // if end is the last chunk, set last chunk to start->prev
  if (heap_chunk_last == end)
    heap_chunk_last = __heap_chunk_meta_prev(start);

  // setup start chunk's metadata
  start->meta[0] = HEAP_CHUNK_MAGIC;
  start->meta[1] = total_size;

  return (void *)start + HEAP_CHUNK_META_SIZE;
}

void *heap_realloc(void *mem, uint64_t size) {
  struct heap_chunk *realloc_start = NULL, *realloc_end = NULL;
  struct heap_chunk *start = NULL, *cur = NULL;
  uint64_t           total_size = 0;

  start       = mem - HEAP_CHUNK_META_SIZE;
  realloc_end = mem + __heap_chunk_meta_size(start) - sizeof(struct heap_chunk);

  if (HEAP_CHUNK_META_SIZE > (uint64_t)mem || !__heap_chunk_is_magical(start)) {
    panic("Attempt to reallocate an invalid chunk");
    return NULL;
  }

  if ((total_size = __heap_chunk_meta_size(start)) >= size)
    return mem;

  // attempt to extend the memory buffer by moving the end chunk
  for (cur = heap_chunk_first; size > total_size && cur != NULL; cur = __heap_chunk_meta_next(cur)) {
    /*

     * check if the current chunk is contiguous with the end chunk
     * if so move end chunk to current chunk and extend the size of
     * the memory buffer by a single chunk

    */

    if (realloc_end + 1 != cur)
      continue;

    realloc_end = cur;
    total_size += sizeof(struct heap_chunk);

    if (NULL == realloc_start)
      realloc_start = realloc_end;
  }

  /*

   * if we fail to extend the buffer by moving the end chunk then we'll
   * allocate a new buffer with the new size and copy old buffer's content
   * to the new buffer, then heap_free() the old buffer

  */
  if (size > total_size) {
    cur = heap_alloc(size);
    memcpy(cur, mem, __heap_chunk_meta_size(start));
    heap_free(mem);
    return cur;
  }

  /*

   * this part is similar to heap_alloc(), we just remove references
   * to the new chunks we allocated

  */
  if ((cur = __heap_chunk_meta_prev(realloc_start)) != NULL)
    __heap_chunk_meta_next_set(cur, __heap_chunk_meta_next(realloc_end));

  if ((cur = __heap_chunk_meta_next(realloc_end)) != NULL)
    __heap_chunk_meta_prev_set(cur, __heap_chunk_meta_prev(realloc_start));

  if (heap_chunk_first == realloc_end)
    heap_chunk_first = __heap_chunk_meta_next(realloc_end);

  if (heap_chunk_last == realloc_start)
    heap_chunk_last = __heap_chunk_meta_prev(realloc_start);

  // update the size of the chunk
  __heap_chunk_meta_size(start) = total_size;
  return mem;
}

void heap_free(void *mem) {
  struct heap_chunk *start = NULL, *end = NULL, *cur = NULL;

  start = mem - HEAP_CHUNK_META_SIZE;
  end   = mem + __heap_chunk_meta_size(start) - sizeof(struct heap_chunk);

  if (HEAP_CHUNK_META_SIZE > (uint64_t)mem || !__heap_chunk_is_magical(start))
    return panic("Attempt to free an invalid chunk");

  for (cur = start; cur <= end; cur++) {
    if (cur != start)
      __heap_chunk_meta_prev_set(cur, cur - 1);
    __heap_chunk_meta_next_set(cur, cur + 1);
  }

  __heap_chunk_meta_prev_set(start, NULL);
  __heap_chunk_meta_next_set(end, NULL);

  for (cur = heap_chunk_first; cur != NULL; cur = __heap_chunk_meta_next(cur)) {
    if (start > cur)
      __heap_chunk_meta_prev_set(start, cur);

    if (__heap_chunk_meta_prev(start) == __heap_chunk_meta_prev(cur) && end < cur)
      __heap_chunk_meta_next_set(end, cur);
  }

  if ((cur = __heap_chunk_meta_prev(start)) != NULL)
    __heap_chunk_meta_next_set(cur, start);
  else
    heap_chunk_first = start;

  if ((cur = __heap_chunk_meta_next(end)) != NULL)
    __heap_chunk_meta_prev_set(cur, end);
  else
    heap_chunk_last = end;
}
