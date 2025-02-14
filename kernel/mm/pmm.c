#include "boot/boot.h"
#include "boot/multiboot.h"

#include "util/printk.h"
#include "util/mem.h"

#include "mm/pmm.h"
#include "mm/vmm.h"

#include "errno.h"

#define pmm_fail(f, ...) pfail("PMM: " f, ##__VA_ARGS__)
#define pmm_warn(f, ...) pwarn("PMM: " f, ##__VA_ARGS__)
#define pmm_debg(f, ...) pdebg("PMM: " f, ##__VA_ARGS__)

// describes a posion in the bitmap
typedef struct {
  uint8_t  bit;
  uint64_t index;
} pmm_bm_pos_t;

// describes a memory region/area
struct pmm_reg {
  uint64_t start, end, pos;
};

struct multiboot_tag_mmap *pmm_mmap_tag = NULL;            // mmap multiboot tag
uint64_t                  *pmm_bm = NULL, pmm_bm_size = 0; // used to store the bitmap address and size
struct pmm_reg             pmm_reg_known[] =
    {
        {0xA0000, 0xBFFFF}, // VGA, https://wiki.osdev.org/VGA_Hardware
        {0,       0      }
},
               pmm_reg_free; // stores the free memory region

#define __pmm_align_to_page(num, inc)                                                                                  \
  do {                                                                                                                 \
    while (num % VMM_PAGE_SIZE != 0)                                                                                   \
      num += inc;                                                                                                      \
  } while (0)

#define __pmm_foreach_mb_mmap_entry()                                                                                  \
  for (multiboot_memory_map_t *map = (void *)&pmm_mmap_tag->entries[0];                                                \
      pmm_mmap_tag->size > (void *)map - (void *)&pmm_mmap_tag->entries[0];                                            \
      map++)

#define __pmm_foreach_reg_known() for (struct pmm_reg *reg = &pmm_reg_known[0]; reg->start != 0 || reg->end != 0; reg++)

#define __pmm_reg_size(reg)           ((reg)->end - (reg)->start)
#define __pmm_reg_pos_reset(reg)      ((reg)->pos = (reg)->start)
#define __pmm_reg_contains(reg, addr) ((reg)->end > addr && addr >= (reg)->start)
#define __pmm_reg_is_valid(reg)       ((reg)->end > (reg)->start && (reg)->end >= (reg)->pos && (reg)->pos >= (reg)->start)
#define __pmm_reg_align(reg)                                                                                           \
  do {                                                                                                                 \
    __pmm_align_to_page((reg)->start, 1);                                                                              \
    __pmm_align_to_page((reg)->end, -1);                                                                               \
  } while (0);

#define PMM_BM_ENTRY_BIT_SIZE    (sizeof(uint64_t) * 8)
#define __pmm_bm_is_ready()      (pmm_bm != NULL && pmm_bm_size != 0)
#define __pmm_bm_get_max_index() (pmm_bm_size / 8)
#define __pmm_bm_pos_reset(pos)  bzero(pos, sizeof(pmm_bm_pos_t));
#define __pmm_bm_pos_to_addr(pos)                                                                                      \
  (pmm_reg_free.start + ((pos)->bit + (pos)->index * PMM_BM_ENTRY_BIT_SIZE) * VMM_PAGE_SIZE)
#define __pmm_bm_pos_set(pos)      (pmm_bm[(pos)->index] |= 1 << (pos)->bit)
#define __pmm_bm_pos_clear(pos)    (pmm_bm[(pos)->index] &= ~(1 << (pos)->bit))
#define __pmm_bm_pos_get(pos)      ((pmm_bm[(pos)->index] >> (pos)->bit) & 1)
#define __pmm_bm_pos_is_valid(pos) (__pmm_bm_get_max_index() > (pos)->index)

bool __pmm_bm_pos_from_addr(pmm_bm_pos_t *pos, uint64_t addr) {
  if (NULL == pos || !__pmm_bm_is_ready())
    return false;

  addr -= pmm_reg_free.start;
  addr /= VMM_PAGE_SIZE;

  pos->index = addr / PMM_BM_ENTRY_BIT_SIZE;
  pos->bit   = addr % PMM_BM_ENTRY_BIT_SIZE;

  return __pmm_bm_pos_is_valid(pos);
}

int32_t __pmm_bm_pos_next(pmm_bm_pos_t *pos) {
  if (!__pmm_bm_is_ready())
    return -EFAULT;

  // if we reached the end of the current uint64_t, move onto next one
  if (++pos->bit >= PMM_BM_ENTRY_BIT_SIZE) {
    pos->bit = 0;
    pos->index++;
  }

  // check we reached the end
  if (pos->index >= __pmm_bm_get_max_index())
    return -ERANGE;

  // return the value stored in the new position
  return __pmm_bm_pos_get(pos);
}

bool __pmm_is_free_memory(uint64_t addr) {
  // check if the address is a known memory region
  __pmm_foreach_reg_known() {
    if (__pmm_reg_contains(reg, addr))
      return false;
  }

  // check if the adddress is in a available (free) memory region
  __pmm_foreach_mb_mmap_entry() {
    // we should only check the available mmap tags
    if (MULTIBOOT_MEMORY_AVAILABLE != map->type)
      continue;

    // check if address is in the range of this mmap tag
    if (addr < map->addr || addr + VMM_PAGE_SIZE > map->addr + map->len)
      continue;

    return true;
  }

  return false;
}

int32_t pmm_init() {
  // make sure the structure that stores free memory region is clean
  bzero(&pmm_reg_free, sizeof(pmm_reg_free));

  // attempt to find the mmap multiboot info tag
  if (NULL == (pmm_mmap_tag = mb_get(MULTIBOOT_TAG_TYPE_MMAP))) {
    pmm_fail("cannot find the mmap multiboot info tag");
    return -EFAULT;
  }

  // find the start and the end of the available memory
  __pmm_foreach_mb_mmap_entry() {
    if (MULTIBOOT_MEMORY_AVAILABLE != map->type)
      continue;

    pmm_debg("available mmap entry 0x%p - 0x%p", map->addr, map->addr + map->len);

    if (map->addr < pmm_reg_free.start)
      pmm_reg_free.start = map->addr;

    if (map->addr + map->len > pmm_reg_free.end)
      pmm_reg_free.end = map->addr + map->len;
  }

  // available memory start address should not point to kernel binary's memory
  if (BOOT_KERNEL_END_PADDR > pmm_reg_free.start)
    pmm_reg_free.start = BOOT_KERNEL_END_PADDR;

  // align the addresses to page boundaries & set the position
  __pmm_reg_align(&pmm_reg_free);
  __pmm_reg_pos_reset(&pmm_reg_free);

  // make sure end > start (otherwise we don't have memory)
  if (!__pmm_reg_is_valid(&pmm_reg_free)) {
    pmm_fail("no available physical memory");
    return -ENOMEM;
  }

  // calculate the bitmap size for pmm_mem_start to pmm_mem_end
  pmm_bm_size = __pmm_reg_size(&pmm_reg_free) / VMM_PAGE_SIZE / 8;
  __pmm_align_to_page(pmm_bm_size, 1);

  pmm_debg("bitmapping 0x%p - 0x%p with %u bytes", pmm_reg_free.start, pmm_reg_free.end, pmm_bm_size);

  // allocate memory for the bitmap
  if ((pmm_bm = vmm_map(pmm_bm_size / VMM_PAGE_SIZE, VMM_FLAGS_DEFAULT)) == NULL) {
    pmm_fail("failed to allocate the bitmap (size: %u)", pmm_bm_size);
    return -EFAULT;
  }

  // clear out the bitmap
  bzero(pmm_bm, pmm_bm_size);

  /*

   * we may have allocations that we made without a bitmap
   * these allocations increment the pmm_reg_free.pos, so we need to set
   * all the pages starting from pmm_reg_free.start to pmm_reg_free.pos as
   * not available (1) so they won't be allocated again and we'll be able to free them

  */
  pmm_bm_pos_t pos;
  __pmm_bm_pos_reset(&pos);

  for (; pmm_reg_free.pos > __pmm_bm_pos_to_addr(&pos); __pmm_bm_pos_next(&pos))
    __pmm_bm_pos_set(&pos);

  return 0;
}

uint64_t __pmm_alloc_no_bm(uint64_t num, uint64_t align) {
  uint64_t pos = pmm_reg_free.pos, start = 0, cur = 0;

  for (; num > cur; pos += VMM_PAGE_SIZE) {
    if (pos >= pmm_reg_free.end)
      break;

    if (cur == 0 && (start = pos) % (align ? align : 1) != 0)
      continue;

    if (!__pmm_is_free_memory(pos))
      cur = 0;
    else
      cur++;
  }

  if (cur != num) {
    pmm_fail("failed to allocate %u pages (no bitmap allocation)", num);
    return NULL;
  }

  pmm_reg_free.pos = pos;
  return start;
}

uint64_t pmm_alloc(uint64_t num, uint64_t align) {
  if (align != 0 && ((VMM_PAGE_SIZE > align && VMM_PAGE_SIZE % align != 0) ||
                        (align > VMM_PAGE_SIZE && align % VMM_PAGE_SIZE != 0))) {
    pmm_fail("requested invalid alignment (0x%x)", align);
    return NULL;
  }

  if (!__pmm_bm_is_ready())
    return __pmm_alloc_no_bm(num, align);

  uint64_t     start = 0, cur = 0;
  int32_t      val = 0;
  pmm_bm_pos_t pos;

  __pmm_bm_pos_reset(&pos);

  for (val = __pmm_bm_pos_get(&pos); num > cur; val = __pmm_bm_pos_next(&pos)) {
    // check if we were able to get the next entry in the bitmap
    if (val < 0)
      break;

    if (cur == 0 && (start = __pmm_bm_pos_to_addr(&pos)) % (align ? align : 1) != 0)
      continue;

    // if the next entry is used reset the cur counter
    if (val || !__pmm_is_free_memory(__pmm_bm_pos_to_addr(&pos)))
      cur = 0;
    else
      cur++;
  }

  if (cur != num) {
    pmm_fail("failed to allocate %u pages", num);
    return NULL;
  }

  // make sure the pages we are allocating are set as not available (1) in the bitmap
  for (__pmm_bm_pos_from_addr(&pos, start), cur = 0; num > cur; cur++, __pmm_bm_pos_next(&pos))
    __pmm_bm_pos_set(&pos);

  return start;
}

bool pmm_is_allocated(uint64_t paddr) {
  pmm_bm_pos_t pos;

  // try to obtain the bitmap position from the given address
  if (!__pmm_bm_pos_from_addr(&pos, paddr))
    return false;

  // return the value stored in that position (1 = allocted, 0 = free)
  return __pmm_bm_pos_get(&pos);
}

int32_t pmm_free(uint64_t paddr, uint64_t num) {
  pmm_bm_pos_t pos;
  int32_t      val = 0;

  // try to obtain the bitmap position from the given address
  if (!__pmm_bm_pos_from_addr(&pos, paddr))
    return -EFAULT;

  // loop through "num" pages and mark them as free in the bitmap
  for (val = __pmm_bm_pos_get(&pos); num > 0; num--, val = __pmm_bm_pos_next(&pos)) {
    // if we reach the end of the bitmap, fail with a warning
    if (val < 0) {
      pmm_warn("attempted to free a page that is not in the bitmap");
      return -ERANGE;
    }

    // if the page is already marked as free, also fail with a warning
    if (!val) {
      pmm_warn("attempted double free (0x%p)", __pmm_bm_pos_to_addr(&pos));
      return -EFAULT;
    }

    // otherwise mark the page as free (0) in the bitmap
    __pmm_bm_pos_clear(&pos);
  }

  return 0;
}
