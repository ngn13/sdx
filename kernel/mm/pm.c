#include "mm/pm.h"

#include "util/bit.h"
#include "util/mem.h"
#include "util/printk.h"

/*

 * paging table pointer functions

 * table pointer is just a integer that stores the offsets for
 * diffrent tables which when combined together resolve into a certain address

*/
typedef uint64_t pm_tp_t;

#define PM_ENTRY_ADDR_MASK 0xfffffffffffff000 // mask used to obtain only the address from the entry
#define PM_ENTRY_AVAIL_BIT 9

#define pm_tp_set_level(p, l, v) (p = p & ~((uint64_t)UINT16_MAX << (l * 16)) | ((uint64_t)v << (l * 16)))
#define pm_tp_get_level(p, l)    ((p >> (l * 16)) & UINT16_MAX)

#define pm_tp_pml4(p) pm_tp_get_level(p, 0)
#define pm_tp_pdpt(p) pm_tp_get_level(p, 1)
#define pm_tp_pdt(p)  pm_tp_get_level(p, 2)
#define pm_tp_pt(p)   pm_tp_get_level(p, 3)

// prints a table pointer (used for logging)
void __pm_tp_print(pm_tp_t tp) {
  printf("(%d/%d/%d/%d)\n", pm_tp_pml4(tp), pm_tp_pdpt(tp), pm_tp_pdt(tp), pm_tp_pt(tp));
}

// creates a table pointer from a memory address
pm_tp_t pm_tp(uint64_t addr) {
  pm_tp_t res = 0;

  pm_tp_set_level(res, 0, ((addr >> 39) & 0x1FF));
  pm_tp_set_level(res, 1, ((addr >> 30) & 0x1FF));
  pm_tp_set_level(res, 2, ((addr >> 21) & 0x1FF));
  pm_tp_set_level(res, 3, ((addr >> 12) & 0x1FF));

  return res;
}

// calculates the next table pointer recursively
pm_tp_t __pm_tp_next(pm_tp_t tp, uint16_t level) {
  uint16_t cur = 0;

  if ((cur = pm_tp_get_level(tp, level) + 1) < PM_ENTRY_MAX)
    return pm_tp_set_level(tp, level, cur);

  if (level == 0)
    return tp;

  return __pm_tp_next(tp, (level - 1));
}

// returns the next table pointer (next page) from a table pointer
pm_tp_t pm_tp_next(pm_tp_t tp) {
  return __pm_tp_next(tp, PM_LEVEL - 1);
}

// calculates the previous table pointer recursively
pm_tp_t __pm_tp_pre(pm_tp_t tp, uint16_t level) {
  uint16_t cur = 0;

  if ((cur = pm_tp_get_level(tp, level) - 1) > 0)
    return pm_tp_set_level(tp, level, cur);

  if (level == 0)
    return tp;

  return __pm_tp_pre(tp, (level - 1));
}

// returns the previous table pointer (next page) from a table pointer
pm_tp_t pm_tp_pre(pm_tp_t tp) {
  return __pm_tp_pre(tp, PM_LEVEL - 1);
}

uint64_t *__pm_tp_entry(pm_tp_t tp, uint64_t *table, uint16_t cur_level, uint16_t dst_level) {
  if (PM_LEVEL - 1 == cur_level || dst_level - 1 == cur_level)
    return &table[pm_tp_get_level(tp, cur_level)];
  return __pm_tp_entry(
      tp, (uint64_t *)(table[pm_tp_get_level(tp, cur_level)] & PM_ENTRY_ADDR_MASK), (cur_level + 1), dst_level);
}

// get the specified level entry from a table pointer
uint64_t *pm_tp_entry_at(pm_tp_t tp, uint16_t level) {
  uint64_t *start = (uint64_t *)pm_start;
  return __pm_tp_entry(tp, start, 0, level);
}

// get PT entry from a table pointer
#define pm_tp_entry(p) pm_tp_entry_at(p, PM_LEVEL)

// recursively calculates the min table size that a pointer can fit in
uint64_t __pm_tp_size_min(pm_tp_t tp, uint16_t level, bool last) {
  uint64_t size = PM_PAGE_SIZE;

  if (PM_LEVEL - 1 == level)
    return size;

  uint64_t pc = last ? pm_tp_get_level(tp, level) + 1 : PM_ENTRY_MAX;
  uint16_t i  = 0;

  for (; i < pc; i++) {
    if (last && i == pm_tp_get_level(tp, level))
      size += __pm_tp_size_min(tp, level + 1, true);
    else
      size += __pm_tp_size_min(tp, level + 1, false);
  }

  return size;
}

// returns the minimum table size that a table pointer can fit in
uint64_t pm_tp_size_min(pm_tp_t tp) {
  return __pm_tp_size_min(tp, 0, true);
}

/*

 * paging functions & structures
 * these are used to manage paging tables and indiviual pages

*/
struct pm_state {
  pm_tp_t  avail_start; // where the available memory starts
  pm_tp_t  avail_end;   // where the available memory ends
  uint64_t alloced;     // allocated page count
};

struct pm_state pm_st;

// returns true if the given address is mapped
bool pm_is_mapped(uint64_t addr) {
  // return pm_mapped > addr;
  return false;
}

// checks if the paging is setup correctly, and load the available memory start address
bool pm_init(uint64_t start, uint64_t end) {
  /*
  if (mb_mem_avail_limit == 0) {
    printk(KERN_FAIL, "PM: invalid available memory address limit");
    return false;
  }

  if (pm_mapped == 0) {
    printk(KERN_FAIL, "PM: invalid last mapped address");
    return false;
  }

  pm_tp_t tp_limit = pm_tp(mb_mem_avail_limit);
  pm_tp_t tp_paged = pm_tp(pm_mapped);

  printk(KERN_INFO, "PM: available memory ends at 0x%p ", mb_mem_avail_limit);
  __pm_tp_print(tp_limit);

  printk(KERN_INFO, "PM: paged memory ends at 0x%p ", pm_mapped);
  __pm_tp_print(tp_paged);

  if (pm_mapped < mb_mem_avail_limit)
    printk(KERN_WARN,
        "PM: mapped %l MBs of %l MBs, not using all the available memory\n",
        pm_mapped / 1024 / 1024,
        mb_mem_avail_limit / 1024 / 1024);

  bzero(&pm_st, sizeof(pm_st));

  pm_st.avail_start = pm_tp(start);              // table pointer for the first (available) page
  pm_st.avail_end   = pm_tp(end - PM_PAGE_SIZE); // table pointer for the last (available) page
  */

  return false;
}

// recursive paging extend function
uint64_t *__pm_extend(pm_tp_t tp, uint64_t *addr, uint64_t *mem, uint16_t level, bool last) {
  uint64_t *table = (uint64_t *)*addr;
  uint16_t  i     = 0;

  *addr += PM_PAGE_SIZE;

  if (PM_LEVEL - 1 == level) {
    for (; i < PM_ENTRY_MAX; i++) {
      if (table[i] != 0)
        continue;

      table[i] = *mem | PM_ENTRY_FLAGS_DEFAULT;
      *mem += PM_PAGE_SIZE;
    }
    return table;
  }

  uint64_t page_count = last ? pm_tp_get_level(tp, level) + 1 : PM_ENTRY_MAX;
  uint64_t table_addr = 0;

  for (; i < page_count; i++) {
    if (last && i == pm_tp_get_level(tp, level))
      table_addr = (uint64_t)__pm_extend(tp, addr, mem, level + 1, true);
    else
      table_addr = (uint64_t)__pm_extend(tp, addr, mem, level + 1, false);

    if (table[i] == 0)
      table[i] = table_addr | PM_ENTRY_FLAGS_DEFAULT;
  }

  return table;
}

// extends the page table until it maps the given address
bool pm_extend(uint64_t addr) {
  /*
  // is the address already mapped?
  if (pm_is_mapped(addr))
    return true;

  // printk(KERN_INFO, "paging: extending page memory to 0x%p\n", addr);

  pm_tp_t  end   = pm_tp(addr);
  uint64_t start = pm_start;

  // recursively extend the map
  __pm_extend(end, &start, &pm_mapped, 0, true);

  pm_tp_t tp_paged = pm_tp(pm_mapped);

  if (printk(KERN_DEBG, "PM: extended paged memory to 0x%p ", pm_mapped) != 0)
    __pm_tp_print(tp_paged);

  return true;*/
  return false;
}

// set or clear page entry flags for a given table pointer
bool __pm_flags(pm_tp_t tp, uint64_t flags, bool do_clear, bool all_levels) {
  uint64_t *entry = NULL;
  uint16_t  level = PM_LEVEL;

  for (; level > 0 && (all_levels || level == 0); level--) {
    entry = pm_tp_entry_at(tp, level);

    if (do_clear)
      *entry &= ~flags;
    else
      *entry |= flags;
  }

  return true;
}

// set or clear page entry flags for "count" pages starting at "addr"
bool pm_flags(uint64_t addr, uint64_t count, uint64_t flags, bool do_clear, bool all_levels) {
  if (count == 0)
    return false;

  for (pm_tp_t tp = pm_tp(addr); count > 0; tp = pm_tp_next(tp), count--)
    __pm_flags(tp, flags, do_clear, all_levels);

  return true;
}

// allocates "count" amount of pages
void *pm_alloc(uint64_t count) {
  // we can't allocate 0 pages
  if (count == 0)
    return false;

  uint64_t res = 0, found = 0, *entry = NULL;
  pm_tp_t  cur = pm_st.avail_end; // current page table pointer

  for (; cur != pm_st.avail_start; cur = pm_tp_pre(cur)) {
    entry = pm_tp_entry(cur);

    // is the page available?
    if (bit_get(*entry, PM_ENTRY_AVAIL_BIT) == 1) {
      found = 0;
      continue;
    }

    found++;
    res = (*entry & PM_ENTRY_ADDR_MASK);

    if (count == found)
      break;
  }

  /*

   * if the amount of pages that have been found is not equal
   * to the count of pages requested, then the allocation fails

  */
  if (found != count)
    return NULL; // allocation failed

  // set all the found pages as no longer available
  pm_tp_t page = pm_tp(res);

  for (; found > 0; found--) {
    entry  = pm_tp_entry(page);
    *entry = bit_set(*entry, PM_ENTRY_AVAIL_BIT, 1);
    *entry &= PM_ENTRY_FLAGS_CLEAR;   // clear all the flags
    *entry |= PM_ENTRY_FLAGS_DEFAULT; // and set the defaults
    page = pm_tp_next(page);
  }

  return (void *)res;
}

// free "count" amount of pages, first page is pointed by "mem"
void pm_free(void *mem, uint64_t count) {
  if (NULL == mem || count == 0)
    return;

  pm_tp_t   page  = pm_tp((uint64_t)mem);
  uint64_t *entry = NULL;

  // free all the request pages
  for (; count > 0; count--) {
    entry  = pm_tp_entry(page);
    *entry = bit_set(*entry, PM_ENTRY_AVAIL_BIT, 0);
    page   = pm_tp_next(page);
  }
}
