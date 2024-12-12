#include "core/disk.h"
#include "core/mbr.h"
#include "fs/vfs.h"

#include "util/list.h"
#include "util/math.h"
#include "util/mem.h"
#include "util/panic.h"

#include "core/ahci.h"
#include "mm/vmm.h"

#include "config.h"

#define DISK_DEFAULT_SECTOR_SIZE 512
disk_t *disk_first = NULL;

disk_t *disk_add(disk_controller_t controller, void *data) {
  if (NULL == data)
    return false;

  disk_t *new = vmm_alloc(sizeof(disk_t));
  bzero(new, sizeof(disk_t));

  new->controller  = controller;
  new->data        = data;
  new->sector_size = DISK_DEFAULT_SECTOR_SIZE;

  slist_add(&disk_first, new, disk_t);
  pdebg("Disk: Added a new disk device (Address: 0x%x Controller: %d)", new, new->controller);

  return new;
}

void disk_remove(disk_t *disk) {
  if (NULL == disk || NULL == disk_first)
    return;

  slist_del(&disk_first, disk, disk_t);
  vmm_free(disk);

  return;
}

bool disk_do(disk_t *disk, disk_op_t op, uint64_t lba, uint64_t sector_count, uint8_t *buf) {
  typedef bool port_do_t(void *, disk_op_t, uint64_t, uint64_t, uint8_t *);

  port_do_t *port_do = NULL;

  switch (disk->controller) {
  case DISK_CONTROLLER_AHCI:
    port_do = (void *)ahci_port_do;
    break;

  default:
    disk_fail("unknown controller (%d)", disk->controller);
    panic("Encountered a disk with an unknown controller");
    return false;
  }

  return port_do(disk->data, op, lba, sector_count, buf);
}

bool disk_read_lba(disk_t *disk, uint64_t lba, uint64_t size, uint8_t *buf) {
  uint64_t rem_size = 0;

  if ((rem_size = size % disk->sector_size) == 0)
    return disk_do(disk, DISK_OP_READ, lba, div_floor(size, disk->sector_size), buf);

  uint64_t buf_offset = 0;

  if (size < disk->sector_size)
    goto do_copy;

  while (size != rem_size) {
    if (!disk_do(disk, DISK_OP_READ, lba++, 1, buf + buf_offset))
      return false;
    size -= disk->sector_size;
    buf_offset += disk->sector_size;
  }

do_copy:
  if (rem_size == 0)
    return true;

  uint8_t rem_buf[disk->sector_size];
  bool    ret = false;

  ret = disk_do(disk, DISK_OP_READ, lba, 1, rem_buf);

  memcpy(buf + buf_offset, rem_buf, rem_size);
  return ret;
}

bool disk_read(disk_t *disk, uint64_t offset, uint64_t size, uint8_t *buf) {
  uint64_t rem_offset = 0, lba = div_floor(offset, disk->sector_size);

  if ((rem_offset = offset % disk->sector_size) == 0)
    return disk_read_lba(disk, lba, size, buf);

  uint8_t full_buf[rem_offset + size];
  bool    ret = false;

  ret = disk_read_lba(disk, lba, rem_offset + size, full_buf);

  memcpy(buf, full_buf + rem_offset, size);
  return ret;
}

bool disk_write_lba(disk_t *disk, uint64_t lba, uint64_t size, uint8_t *buf) {
  if (disk->sector_size % size != 0) {
    disk_fail("invalid size for the write operation: %u", size);
    return false;
  }

  return disk_do(disk, DISK_OP_WRITE, lba, div_floor(size, disk->sector_size), buf);
}

bool disk_write(disk_t *disk, uint64_t offset, uint64_t size, uint8_t *buf) {
  if (disk->sector_size % offset != 0) {
    disk_fail("invalid offset for the write operation: %u", offset);
    return false;
  }

  return disk_write_lba(disk, div_floor(offset, disk->sector_size), size, buf);
}

disk_part_t *disk_next(disk_part_t *pre) {
  disk_part_t *found = NULL;
  disk_t      *cur   = NULL;

  if (pre != NULL && (found = pre->next) != NULL)
    goto return_found;

  cur = NULL == pre ? disk_first : pre->disk->next;

  while (NULL != cur) {
    if ((found = cur->parts) != NULL)
      goto return_found;
    cur = cur->next;
  }

return_found:
  if (NULL == found)
    return NULL;

  if (!found->available)
    return disk_next(found);

  return found;
}
