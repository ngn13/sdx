#include "core/acpi.h"
#include "boot/multiboot.h"

#include "core/driver.h"
#include "limits.h"
#include "mm/paging.h"
#include "util/printk.h"
#include "util/string.h"
#include "util/list.h"
#include "util/mem.h"

#include "mm/heap.h"
#include "mm/vmm.h"

#include "types.h"
#include "errno.h"

#define acpi_info(f, ...) pinfo("ACPI: " f, ##__VA_ARGS__)
#define acpi_fail(f, ...) pfail("ACPI: " f, ##__VA_ARGS__)
#define acpi_debg(f, ...) pdebg("ACPI: " f, ##__VA_ARGS__)

driver_new(acpi, acpi_load, acpi_unload);

/*

 * ACPI 1.0 uses RSDP (root system descriptor pointer), which as you
 * might have guessed, points to the root system descriptor table, which
 * is RDST (see the sdt_header structure for more info)

*/
struct rsdp {
  char     sig[8];
  uint8_t  checksum;
  char     oem_id[6];
  uint8_t  revision;
  uint32_t rsdt_addr;
} __attribute__((packed));

/*

 * ACPI 2.0 and later uses XSDP (extended system descriptor pointer), similar
 * to the RSDP, this points to the extended system descriptor table, XSDT

 * it has the same format with the RSDP at the start, this is for compability, but
 * the rest of the structure is extended to contain the XSDT pointer and more info

*/
struct xsdp {
  struct rsdp rsdp;
  uint32_t    len;
  uint64_t    xsdt_addr;
  uint8_t     extended_checksum;
  uint8_t     reserved[3];
} __attribute__((packed));

/*

 * describes the header of a system table descriptor, each descriptor stores different
 * information, difference between the RSDT and XSDT decriptors are that RSDT
 * descriptor's use 32 bit pointers, so we need to work on the length accordingly

*/
struct sdt_header {
#define SDT_HEADER_SIG_SIZE (4)
  char     sig[SDT_HEADER_SIG_SIZE];
  uint32_t len;
  uint8_t  revision;
  uint8_t  checksum;
  char     oem_id[6];
  char     oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
};

#define sdt_header_sig_cmp(header, sig) (strncmp(header->sig, sig, SDT_HEADER_SIG_SIZE) == 0)

// root SDT structure (ACPI 1.0)
struct rsdt {
  struct sdt_header header;
  uint32_t          sdt[1];
};

// (extended) root SDT structure (ACPI 2.0 and later)
struct xsdt {
  struct sdt_header header;
  uint64_t          sdt[1];
};

#define rsdt_sdt_count(r) ((r->header.len - sizeof(r->header)) / sizeof(r->sdt[0]))
#define xsdt_sdt_count(x) ((x->header.len - sizeof(x->header)) / sizeof(x->sdt[0]))

// stores previously mapped SDTs
struct sdt_map {
  void              *base;
  uint64_t           size;
  struct sdt_header *sdt;
  struct sdt_map    *next;
};

// these store information about ACPI when ACPI is loaded
struct sdt_map *__acpi_root    = NULL;
uint8_t         __acpi_version = 0;

bool __acpi_check(char *c, uint64_t size) {
  uint8_t sum = 0;

  for (; size > 0; c++, size--)
    sum += *c;

  // sum of every byte should be zero
  return !sum;
}

struct sdt_map *__acpi_sdt_map(uint64_t paddr, uint64_t size) {
  if (paddr == 0 || size == 0)
    return NULL;

  uint64_t        paddr_base = round_down(paddr, PAGE_SIZE);
  struct sdt_map *map        = NULL;

  // allocate a SDT map and make sure the allocation was successful
  if ((map = heap_alloc(sizeof(struct sdt_map))) == NULL) {
    acpi_fail("failed to allocate SDT map for SDT @ 0x%p", paddr);
    return NULL;
  }

  // setup the map
  bzero(map, sizeof(*map));
  map->size = vmm_calc(paddr - paddr_base + size);
  map->base = vmm_map_paddr(paddr_base, map->size, VMM_ATTR_SAVE);

  // make sure map didn't fail
  if (NULL == map->base) {
    acpi_fail("failed to map base 0x%p for SDT @ 0x%p", paddr_base, paddr);
    return NULL;
  }

  // calculate the SDT's virtual address by adding the offset
  map->sdt = map->base + (paddr - paddr_base);

  // add the SDT map to the list and return it
  slist_add_end(&__acpi_root, map, struct sdt_map);
  return map;
}

int32_t __acpi_sdt_remap(struct sdt_map *map, uint64_t size) {
  if (NULL == map || size == 0)
    return -EINVAL;

  uint64_t paddr = 0, offset = 0;
  int32_t  err = 0;

  // calculate the offset
  offset = (void *)map->sdt - map->base;

  // see if we really need to extend the mapping
  if (map->size >= vmm_calc(size + offset))
    return 0;

  // get the original physical address
  paddr = vmm_resolve(map->base);

  // remove the previous mapping
  if ((err = vmm_unmap(map->base, map->size, 0)) != 0) {
    acpi_fail("failed to unmap base 0x%p for SDT @ 0x%p", map->base, map->sdt);
    return err;
  }

  // calculate the new size & and remap
  map->size = vmm_calc(size + offset);
  map->base = vmm_map_paddr(paddr, map->size, VMM_ATTR_SAVE);

  // check if mapping was sucessful
  if (NULL == map->base) {
    acpi_fail("failed to map base 0x%p for SDT @ 0x%p", paddr, paddr + offset);
    return -ENOMEM;
  }

  // calculate the SDT's virtual address by adding the offset
  map->sdt = map->base + offset;
  return 0;
}

#define __acpi_rsdp_check(rsdp) __acpi_check((char *)rsdp, sizeof(struct rsdp))
#define __acpi_sdt_check(sdt)   __acpi_check((char *)sdt, sdt->len)

int32_t __acpi_new_load(void *tag) {
  // TODO: implement
  return -ENOSYS;
}

int32_t __acpi_old_load(void *_tag) {
  struct multiboot_tag_old_acpi *tag  = _tag;
  struct rsdp                   *rsdp = (void *)tag->rsdp;
  int32_t                        err  = 0;

  // check the ACPI version (revision == 0 means 1.0)
  if (rsdp->revision != 0) {
    acpi_debg("provided RSDP is not using version 1.0");
    return -EINVAL;
  }

  // check the RSDP by calculating and checking it's checksum
  if (!__acpi_rsdp_check(rsdp)) {
    acpi_debg("invalid RSDP structure");
    return -EINVAL;
  }

  // map RSDT with the header size (temporary)
  if (NULL == __acpi_sdt_map(rsdp->rsdt_addr, sizeof(struct sdt_header))) {
    acpi_debg("failed to map RSDT @ 0x%p", rsdp->rsdt_addr);
    return -EFAULT;
  }

  // remap with the actual size
  if ((err = __acpi_sdt_remap(__acpi_root, __acpi_root->sdt->len)) != 0) {
    acpi_debg("failed to remap RSDT @ 0x%p: %s", rsdp->rsdt_addr, strerror(err));
    return -EFAULT;
  }

  // check the system descriptor table
  if (!__acpi_sdt_check(__acpi_root->sdt)) {
    acpi_debg("failed to check the RSDT");
    return -EINVAL;
  }

  // load all the other SDTs contained in RSDT
  struct rsdt *rsdt = (void *)__acpi_root->sdt;
  uint64_t     cur  = NULL;

  for (uint64_t i = 0; i < rsdt_sdt_count(rsdt); i++) {
    // get the physical SDT pointer
    if ((cur = (uint64_t)rsdt->sdt[i]) == 0) {
      acpi_debg("SDT at %u is a NULL pointer", i);
      continue;
    }

    // map the SDT (which will add it to the list)
    if (NULL == __acpi_sdt_map(cur, sizeof(struct sdt_header)))
      acpi_fail("failed to map SDT @ 0x%p", cur);
  }

  return 0;
}

int32_t acpi_load() {
  /*

   * get the multiboot tags, which contain a copy of the old
   * and the new RSDP/XSDP at the end of the tag

  */
  void   *mb_acpi_old = mb_get(MULTIBOOT_TAG_TYPE_ACPI_OLD);
  void   *mb_acpi_new = mb_get(MULTIBOOT_TAG_TYPE_ACPI_NEW);
  int32_t err         = -1;

  // first, try to load the new ACPI (2.0 and later)
  if (NULL != mb_acpi_new) {
    __acpi_version = ACPI_VERSION_2;
    err            = __acpi_new_load(mb_acpi_new);
  }

  // if fails, try to load the old ACPI (1.0)
  if (err < 0 && NULL != mb_acpi_old) {
    __acpi_version = ACPI_VERSION_1;
    err            = __acpi_old_load(mb_acpi_old);
  }

  else {
    acpi_fail("no available tag, not supported");
    return -EINVAL;
  }

  if (err == 0)
    acpi_info("loaded version %u.0", __acpi_version);

  return err;
}

int32_t acpi_unload() {
  return -ENOSYS;
}

void *acpi_find(char *sig, uint64_t size) {
  // make sure ACPI is loaded & check args
  if (NULL == __acpi_root || !__acpi_version || NULL == sig)
    return NULL;

  int32_t err = 0;

  // check previously mapped SDTs
  slist_foreach(&__acpi_root, struct sdt_map) {
    // compare the signature with the signature we are looking for
    if (!sdt_header_sig_cmp(cur->sdt, sig))
      continue;

    // if we find the SDT, remap it to match with the new size
    if ((err = __acpi_sdt_remap(cur, size)) != 0) {
      acpi_fail("failed to remap SDT @ 0x%p to size %u", cur->sdt, size);
      return NULL;
    }

    // return the found SDT (skip the header)
    return cur->sdt + sizeof(struct sdt_header);
  }

  // not found
  return NULL;
}

int32_t acpi_version() {
  // return an error if ACPI is not loaded or failed to load
  if (NULL == __acpi_root || !__acpi_version)
    return -EFAULT;

  // otherwise return the loaded ACPI version
  return __acpi_version;
}
