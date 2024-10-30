#include "fs/disk.h"
#include "fs/mbr.h"
#include "fs/vfs.h"

#include "util/math.h"
#include "util/mem.h"
#include "util/printk.h"

#include "core/ahci.h"
#include "mm/vmm.h"

#include "config.h"

#define disk_debg(f)        pdebgf("Disk: (0x%x) " f, disk)
#define disk_debgf(f, a...) pdebgf("Disk: (0x%x) " f, disk, a)

#define disk_info(f)        pinfof("Disk: (0x%x) " f, disk)
#define disk_infof(f, a...) pinfof("Disk: (0x%x) " f, disk, a)

#define disk_fail(f)        pfailf("Disk: (0x%x) " f, disk)
#define disk_failf(f, a...) pfailf("Disk: (0x%x) " f, disk, a)

#define DISK_DEFAULT_SECTOR_SIZE 512
disk_t *disk_first = NULL;

disk_part_t *disk_part_next(disk_t *disk, disk_part_t *part) {
  if (NULL == part)
    return disk->parts;
  return part->next;
}

disk_part_t *disk_part_add(disk_t *disk, uint64_t start, uint64_t size) {
#define disk_part_match() (trav->start == start && trav->size == size)

  if (NULL == disk)
    return NULL;

  // check if the partition exists
  disk_part_t *trav = disk->parts, *new = NULL;

  while (trav != NULL) {
    if (disk_part_match())
      return trav;
    trav = trav->next;
  }

  // if not, then create a new one
  trav = disk->parts;

  if (NULL == (new = vmm_alloc(sizeof(disk_part_t))))
    return NULL;

  bzero(new, sizeof(disk_part_t));
  new->start = start;
  new->size  = size;
  new->disk  = disk;

  disk->part_count++;

  if (NULL == trav) {
    disk->parts = new;
    return new;
  }

  while (trav->next != NULL)
    trav = trav->next;

  trav->next = new;
  return new;
}

void __disk_part_block(disk_t *disk) {
  if (NULL == disk)
    return;

  disk_part_t *trav = disk->parts;

  while (NULL != trav) {
    trav->available = false;
    trav            = trav->next;
  }
}

vfs_t *__disk_part_find_vfs(disk_part_t *part) {
  vfs_t *cur = NULL;

  while (NULL != (cur = vfs_next(cur)))
    if (cur->type == VFS_TYPE_DISK && cur->part == part)
      return cur;

  return NULL;
}

void disk_part_clear(disk_t *disk) {
  if (NULL == disk)
    return;

  disk_part_t *trav = disk->parts, *pre = NULL;
  vfs_t       *cur = NULL;

  while (trav != NULL) {
    cur = __disk_part_find_vfs(trav);

    if (trav->available) {
      if (NULL == cur)
        vfs_register(VFS_TYPE_DISK, trav);

      pre  = trav;
      trav = trav->next;
      continue;
    }

    if (NULL != cur)
      vfs_unregister(cur);
    disk->part_count--;

    if (NULL == pre) {
      disk->parts = trav->next;
      vmm_free(trav);
      trav = disk->parts;
      continue;
    }

    pre->next = trav->next;
    vmm_free(trav);
    trav = pre->next;
  }
}

// should be called if the disk gets modified as well
bool disk_scan(disk_t *disk) {
  disk->available = false;
  __disk_part_block(disk);

  if (!disk_do(disk, DISK_OP_INFO, 0, 0, NULL)) {
    disk_fail("failed to load the disk information");
    return false;
  }

#ifdef CONFIG_FS_GPT
#include "fs/gpt.h"
  if (gpt_load(disk)) {
    disk_infof("loaded %d GPT partitions", disk->part_count);
    goto done;
  }
#endif

  if (mbr_load(disk)) {
    disk_infof("loaded %d MBR partitions", disk->part_count);
    goto done;
  }

  disk_fail("failed to load the disk partitions");
  return false;

done:
  disk_part_clear(disk);
  disk->available = true;
  return true;
}

disk_t *disk_add(disk_controller_t controller, void *data) {
  if (NULL == data)
    return false;

  disk_t *new = vmm_alloc(sizeof(disk_t)), *trav = disk_first;
  bzero(new, sizeof(disk_t));

  new->controller  = controller;
  new->data        = data;
  new->sector_size = DISK_DEFAULT_SECTOR_SIZE;

  if (NULL == trav) {
    disk_first = new;
    goto done;
  }

  while (NULL != trav->next)
    trav = trav->next;
  trav->next = new;

done:
  return new;
}

bool disk_remove(disk_t *disk) {
  if (NULL == disk || NULL == disk_first)
    return false;

  disk_t *trav = disk_first;

  if (disk == trav) {
    disk_first = disk_first->next;
    return true;
  }

  while (NULL != trav->next) {
    if (trav->next != disk)
      continue;

    trav->next = trav->next->next;
    vmm_free(disk);
    return true;
  }

  return false;
}

disk_t *disk_next(disk_t *disk) {
  if (NULL == disk)
    return disk_first;
  return disk->next;
}

bool disk_do(disk_t *disk, disk_op_t op, uint64_t offset, uint64_t size, uint8_t *buf) {
  typedef bool port_do_t(void *data, disk_op_t op, uint64_t offset, uint64_t sector_count, uint8_t *buf);
#define sector_count(s) div_floor(s, disk->sector_size)

  port_do_t *port_do = NULL;
  uint64_t   rem = 0, buf_offset = 0;

  switch (disk->controller) {
  case DISK_CONTROLLER_AHCI:
    port_do = (void *)ahci_port_do;
  }

  if ((rem = size % disk->sector_size) == 0)
    return port_do(disk->data, op, offset, sector_count(size), buf);

  if (size < disk->sector_size)
    goto do_copy;

  while (size != rem) {
    if (!port_do(disk->data, op, offset, 1, buf + buf_offset))
      return false;
    size -= disk->sector_size;
    buf_offset += disk->sector_size;
  }

do_copy:
  if (rem == 0)
    return true;

  uint8_t cb[disk->sector_size];
  bool    ret = false;

  ret = port_do(disk->data, op, offset, 1, cb);

  memcpy(buf + buf_offset, cb, rem);
  return ret;
}
