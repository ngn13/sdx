#include "core/disk.h"
#include "core/mbr.h"

#include "fs/fs.h"
#include "mm/vmm.h"

#include "util/list.h"
#include "util/mem.h"

#include "config.h"
#include "limits.h"

disk_part_t *disk_part_add(disk_t *disk, uint64_t start, uint64_t size) {
  if (NULL == disk)
    return NULL;

  disk_part_t *new = NULL;

  // check if the partition exists
  slist_foreach(&disk->parts, disk_part_t) {
    if (cur->start == start && cur->size == size) {
      new = cur;
      break;
    }
  }

  if (NULL != new)
    return new;

  // if not, then create a new one
  if (NULL == (new = vmm_alloc(sizeof(disk_part_t))))
    return NULL;

  bzero(new, sizeof(disk_part_t));
  new->start = start;
  new->size  = size;
  new->disk  = disk;

  slist_add(&disk->parts, new, disk_part_t);

  disk->part_count++;
  return new;
}

void __disk_part_block(disk_t *disk) {
  if (NULL == disk)
    return;

  slist_foreach(&disk->parts, disk_part_t) cur->available = false;
}

void disk_part_clear(disk_t *disk) {
  if (NULL == disk)
    return;

  disk_part_t *trav = disk->parts, *pre = NULL;

  while (trav != NULL) {
    if (trav->available) {
      pre  = trav;
      trav = trav->next;
      continue;
    }

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
bool disk_part_scan(disk_t *disk) {
  disk->available = false;
  __disk_part_block(disk);

  if (!disk_do(disk, DISK_OP_INFO, 0, 0, NULL)) {
    disk_fail("failed to load the disk information");
    return false;
  }

#ifdef CONFIG_CORE_GPT
#include "core/gpt.h"
  if (gpt_load(disk)) {
    disk_info("loaded %d GPT partitions", disk->part_count);
    goto done;
  }
#endif

  if (mbr_load(disk)) {
    disk_info("loaded %d MBR partitions", disk->part_count);
    goto done;
  }

  disk_fail("failed to load the disk partitions");
  return false;

done:
  disk_part_clear(disk);
  disk->available = true;
  return true;
}
