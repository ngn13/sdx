#include "boot/boot.h"
#include "boot/multiboot.h"

#include "util/printk.h"
#include "util/mem.h"
#include "util/asm.h"

#include "mm/pmm.h"
#include "mm/vmm.h"

#include "errno.h"

#define pmm_fail(f, ...) pfail("PMM: " f, ##__VA_ARGS__)
#define pmm_debg(f, ...) pdebg("PMM: " f, ##__VA_ARGS__)

#define __pmm_align_to_page(num, inc)                                                                                  \
  do {                                                                                                                 \
    while (num % VMM_PAGE_SIZE != 0)                                                                                   \
      num += inc;                                                                                                      \
  } while (0)

struct multiboot_tag_mmap *pmm_mmap_tag = NULL; // mmap multiboot tag
#define __pmm_foreach_mb_mmap_entry() for (uint32_t i = 0; pmm_mmap_tag->size > i * pmm_mmap_tag->entry_size; i++)

bool __pmm_is_in_mb_mmap_entry(uint64_t addr) {
  multiboot_memory_map_t *map = NULL;

  __pmm_foreach_mb_mmap_entry() {
    map = (void *)&pmm_mmap_tag->entries[i];

    if (MULTIBOOT_MEMORY_AVAILABLE != map->type)
      continue;

    if (addr < map->addr)
      continue;

    if (addr + VMM_PAGE_SIZE > map->addr + map->len)
      continue;

    return true;
  }

  return false;
}

uint64_t pmm_mem_start = UINT64_MAX; // stores the start address of the memory region that PMM manages
uint64_t pmm_mem_pos   = UINT64_MAX; // stores the start address of the free memory region
uint64_t pmm_mem_end   = 0;          // stores the end address of the memory region that PMM manages
#define __pmm_mem_size() (pmm_mem_end - pmm_mem_start)

uint64_t *pmm_bitmap      = NULL; // bitmap that is used to store free and used pages
uint64_t  pmm_bitmap_size = 0;    // size of the bitmap
#define __pmm_bitmap_get_max_index() (pmm_bitmap_size / 8)
#define PMM_BITMAP_ENTRY_BIT_SIZE    (sizeof(uint64_t) * 8)

typedef struct {
  uint8_t  bit;
  uint64_t index;
} pmm_bitmap_pos_t;

#define __pmm_bitmap_pos_reset(pos) bzero(pos, sizeof(pmm_bitmap_pos_t));

int32_t __pmm_bitmap_pos_next(pmm_bitmap_pos_t *pos) {
  if (NULL == pmm_bitmap)
    return -EFAULT;

  // if we reached the end of the current uint64_t, move onto next one
  if (pos->bit >= PMM_BITMAP_ENTRY_BIT_SIZE) {
    pos->bit = 0;
    pos->index++;
  }

  // check we reached the end
  if (pos->index >= __pmm_bitmap_get_max_index())
    return -ERANGE;

  uint64_t cur = pmm_bitmap[pos->index];
  return (cur >> pos->bit++);
}

int32_t pmm_init() {
  multiboot_memory_map_t *map = NULL;

  if (NULL == (pmm_mmap_tag = mb_get(MULTIBOOT_TAG_TYPE_MMAP)))
    return -EFAULT;

  // find the start and the end of the available memory
  __pmm_foreach_mb_mmap_entry() {
    map = (void *)&pmm_mmap_tag->entries[i];

    if (MULTIBOOT_MEMORY_AVAILABLE != map->type)
      continue;

    pmm_debg("available mmap entry 0x%p - 0x%p", map->addr, map->addr + map->len);

    if (map->addr < pmm_mem_start)
      pmm_mem_start = map->addr;

    if (map->addr + map->len > pmm_mem_end)
      pmm_mem_end = map->addr + map->len;
  }

  // available memory start address should not point to kernel binary's memory
  if (BOOT_KERNEL_END_PADDR > pmm_mem_start)
    pmm_mem_start = BOOT_KERNEL_END_PADDR;

  // align the addresses to page boundaries
  __pmm_align_to_page(pmm_mem_start, 1);
  __pmm_align_to_page(pmm_mem_end, -1);

  // make sure end > start (otherwise we don't have memory)
  if ((pmm_mem_pos = pmm_mem_start) >= pmm_mem_end) {
    pmm_fail("No available physical memory");
    return -ENOMEM;
  }

  // calculate the bitmap size for pmm_mem_start to pmm_mem_end
  pmm_bitmap_size = __pmm_mem_size() / VMM_PAGE_SIZE / 8;
  __pmm_align_to_page(pmm_bitmap_size, 1);

  pmm_debg("bitmapping 0x%p - 0x%p with %u bytes", pmm_mem_start, pmm_mem_end, pmm_bitmap_size);

  // allocate memory for the bitmap
  if ((pmm_bitmap = vmm_map(pmm_bitmap_size / VMM_PAGE_SIZE, VMM_FLAGS_DEFAULT)) == NULL) {
    pmm_fail("failed to allocate the bitmap (size: %u)", pmm_bitmap_size);
    return -EFAULT;
  }

  // clear out the bitmap
  bzero(pmm_bitmap, pmm_bitmap_size);
  return 0;
}

void *__pmm_alloc_no_bitmap(uint64_t num, uint64_t align) {
  while (align != 0 && pmm_mem_pos % align != 0)
    pmm_mem_pos++;

  uint64_t pos = pmm_mem_pos, start = 0, cur = 0;

  for (; num > cur; pos += VMM_PAGE_SIZE) {
    if (pos >= pmm_mem_end)
      break;

    if (cur == 0)
      start = pos;

    if (__pmm_is_in_mb_mmap_entry(pos))
      cur++;
    else
      cur = 0;
  }

  if (cur != num) {
    pmm_fail("failed to allocate %u pages (no bitmap allocation)", num);
    return NULL;
  }

  void *ret   = (void *)start;
  pmm_mem_pos = pos;

  return ret;
}

void *pmm_alloc(uint64_t num, uint64_t align) {
  if (NULL == pmm_bitmap)
    return __pmm_alloc_no_bitmap(num, align);

  uint64_t         start = 0, cur = 0;
  pmm_bitmap_pos_t pos;
  uint8_t          val = 0;

  __pmm_bitmap_pos_reset(&pos);

  // TODO: finish implementation

  while ((val = __pmm_bitmap_pos_next(&pos)) >= 0) {
  }

  return NULL;
}

int32_t pmm_free(void *addr, uint64_t num) {
  return -ENOSYS;
}
