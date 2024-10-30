#pragma once
#include "boot/multiboot.h"
#include "types.h"

// max size of a page/table (we are using the smallest available amount)
#define PM_PAGE_SIZE 4096

// level of paging we are using
#define PM_LEVEL 4

// size of each entry in a table
#define PM_ENTRY_SIZE 8

// maximum amount of entires per table (PM_TABLE_SIZE / PM_ENTRY_SIZE)
#define PM_ENTRY_MAX 512

// paging entry flags (https://wiki.osdev.org/Paging#Page_Directory)
#define PM_ENTRY_FLAG_P   1        // present
#define PM_ENTRY_FLAG_RW  (1 << 1) // read/write
#define PM_ENTRY_FLAG_US  (1 << 2) // user/supervisor
#define PM_ENTRY_FLAG_PWT (1 << 3) // page write through
#define PM_ENTRY_FLAG_PCD (1 << 4) // page cache disable
#define PM_ENTRY_FLAG_A   (1 << 5) // accessed
#define PM_ENTRY_FLAG_D   (1 << 6) // dirty
#define PM_ENTRY_FLAG_PAT (1 << 7) // page attribute table
#define PM_ENTRY_FLAG_G   (1 << 8) // global

// default flags for allocated pages
#define PM_ENTRY_FLAGS_DEFAULT (PM_ENTRY_FLAG_P | PM_ENTRY_FLAG_RW)

#ifndef ASM_FILE

// see boot/paging.S
#define pm_start ((uint64_t)mb_mem_avail_addr) // address for the first paging table (where the page tables start)
extern uint64_t pm_mapped;                     // last address that has been mapped
extern uint32_t pm_end;                        // address for the last paging table (where the page tables end)

typedef struct pm_page {
  uint64_t *pointer;
  uint64_t  address;
  uint64_t  size;
} pm_page_t;

bool pm_init(uint64_t start, uint64_t end);
bool pm_is_mapped(uint64_t addr);
bool pm_extend(uint64_t addr);
bool pm_set(uint64_t addr, uint64_t count, uint16_t flags);

void *pm_alloc(uint64_t count);
void  pm_free(void *mem, uint64_t count);

#endif