#include "fs/vfs.h"

#include "util/mem.h"
#define PPREFIX "VFS:"
#include "util/printk.h"
#include "util/string.h"

#include "config.h"
#include "errno.h"
#include "mm/vmm.h"

#define vfs_debg(f)        pdebgf("(0x%x) " f, vfs)
#define vfs_debgf(f, a...) pdebgf("(0x%x) " f, vfs, a)

#define vfs_fail(f)        pfailf("(0x%x) " f, vfs)
#define vfs_failf(f, a...) pfailf("(0x%x) " f, vfs, a)

#define vfs_info(f)        pinfof("(0x%x) " f, vfs)
#define vfs_infof(f, a...) pinfof("(0x%x) " f, vfs, a)

vfs_t *vfs_list = NULL;

bool __vfs_mount_try_disk(vfs_node_t *node) {
#ifdef CONFIG_FS_FAT32
#include "fs/fat32/fat32.h"
  // try mounting for FAT32
  if (fat32_mount(node))
    return true;
#endif

  /*// try mounting for EXT2
  if(ext2_mount(node))
    return true;*/

  return false;
}

int32_t vfs_mount(vfs_t *vfs, char *path) {
  vfs_debgf("attempting to mount VFS to %s", path);

  int32_t     err  = -ENOENT;
  vfs_node_t *node = vfs_node_new(path);
  node->vfs        = vfs;

  switch (vfs->type) {
  case VFS_TYPE_DISK:
    vfs_debgf("|- Type: %d (disk)", vfs->type);
    vfs_debgf("`- Disk: 0x%x Part: 0x%x Start: %l", vfs->part->disk, vfs->part, vfs->part->start);

    if (__vfs_mount_try_disk(node))
      break;

    err = -ENODEV;
    goto fail;

  default:
    vfs_debgf("unknown VFS type (%d)", vfs->type);
    err = -ENODEV;
    goto fail;
  }

success:
  return 0;
fail:
  vmm_free(node);
  return err;
}

int32_t vfs_umount(char *path) {
  return -ENOSYS;
}

vfs_t *vfs_register(vfs_type_t type, void *data) {
  vfs_t *vfs = NULL, *trav = NULL;

  if (NULL == (vfs = vmm_alloc(sizeof(vfs_t))))
    return NULL;

  // setup the new VFS
  bzero(vfs, sizeof(vfs_t));
  vfs->type = type;

  switch (vfs->type) {
  case VFS_TYPE_DISK:
    vfs_debgf("registered disk VFS (partition: 0x%x)", data);
    vfs->part = data;
    break;

  default:
    vfs_debg("registered unknown VFS");
    break;
  }

  // add it to the list
  if (NULL == (trav = vfs_list)) {
    vfs_list = vfs;
    return vfs;
  }

  while (NULL != trav->next)
    trav = trav->next;

  trav->next = vfs;
  return vfs;
}

int32_t __vfs_umount_all(vfs_t *vfs) {
  return -ENOSYS;
}

int32_t vfs_unregister(vfs_t *vfs) {
  vfs_t *trav = NULL;

  if (vfs == vfs_list) {
    __vfs_umount_all(vfs);
    vfs_list = vfs_list->next;
    goto free;
  }

  if (NULL == (trav = vfs_list))
    return -ENOENT;

  while (vfs != trav->next)
    trav = trav->next;

  __vfs_umount_all(vfs);
  trav->next = trav->next->next;

free:
  vfs_debg("unregistered VFS");
  vmm_free(vfs);
  return 0;
}

vfs_t *vfs_next(vfs_t *cur) {
  if (NULL == cur)
    return vfs_list;
  return cur->next;
}

bool __vfs_check_init(vfs_t *vfs) {
  return true;
}

vfs_t *vfs_detect() {
  vfs_t *trav = vfs_list;

  while (trav != NULL) {
    if (vfs_mount(trav, "/") == 0 && __vfs_check_init(trav)) {
      break;
    }
    trav = trav->next;
  }

  if (NULL == trav)
    pfail("failed to find an available root partition");
  else
    pinfof("(0x%x) detected and mounted the root partition", trav);

  return trav;
}
