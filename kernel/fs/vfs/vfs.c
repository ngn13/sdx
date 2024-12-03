#include "fs/vfs.h"

#include "util/mem.h"
#define PPREFIX "VFS:"
#include "util/list.h"
#include "util/printk.h"
#include "util/string.h"

#include "config.h"
#include "errno.h"
#include "mm/vmm.h"

#define vfs_debg(f, ...) pdebg("(0x%x) " f, vfs, ##__VA_ARGS__)
#define vfs_fail(f, ...) pfail("(0x%x) " f, vfs, ##__VA_ARGS__)
#define vfs_info(f, ...) pinfo("(0x%x) " f, vfs, ##__VA_ARGS__)

vfs_t *head = NULL;

vfs_t *vfs_register(vfs_type_t type, void *type_data) {
  vfs_t *vfs = NULL;

  if (NULL == (vfs = vmm_alloc(sizeof(vfs_t))))
    return NULL;

  // setup the new VFS
  bzero(vfs, sizeof(vfs_t));

  vfs->type      = type;
  vfs->type_data = type_data;

  vfs_debg("registered a new VFS");

  switch (vfs->type) {
  case VFS_TYPE_DISK:
    vfs_debg("|- Type: %d (disk)", vfs->type);
    vfs_debg("`- Disk: 0x%x Part: 0x%x Start: %u", vfs_part(vfs)->disk, vfs_part(vfs), vfs_part(vfs)->start);
    break;

  default:
    vfs_debg("`- Type: %d (unknown)", vfs->type);
    break;
  }

  // add it to the list
  slist_add(&head, vfs, vfs_t);
  return vfs;
}

int32_t __vfs_umount_all(vfs_t *vfs) {
  return -ENOSYS;
}

int32_t vfs_unregister(vfs_t *vfs) {
  slist_del(&head, vfs, vfs_t);
  __vfs_umount_all(vfs);

free:
  vfs_debg("unregistered VFS");
  vmm_free(vfs);
  return 0;
}

vfs_t *vfs_next(vfs_t *cur) {
  return NULL == cur ? head : cur->next;
}
