#include "fs/devfs.h"
#include "mm/heap.h"

#include "util/string.h"
#include "util/list.h"
#include "util/mem.h"

#include "limits.h"
#include "errno.h"
#include "types.h"

struct devfs_device *head = NULL;

struct devfs_device *devfs_device_next(struct devfs_device *dev) {
  if (NULL == dev)
    return head;
  return dev->next;
}

struct devfs_device *devfs_device_from_name(const char *name) {
  if (NULL == name)
    return NULL;

  // search the device list
  slist_foreach(&head, struct devfs_device) {
    if (streq((char *)cur->name, (char *)name))
      return cur;
  }

  return NULL;
}

struct devfs_device *devfs_device_from_addr(int32_t addr) {
  // search the device list
  slist_foreach(&head, struct devfs_device) {
    if (cur->addr == addr)
      return cur;
  }

  return NULL;
}

int32_t devfs_device_register(const char *name, devfs_ops_t *ops, mode_t mode) {
  if (NULL == name || NULL == ops)
    return -EINVAL;

  // make sure there is no other device with the same name
  if (NULL != devfs_device_from_name(name))
    return -EFAULT;

  // allocate the device structure
  struct devfs_device *dev = heap_alloc(sizeof(struct devfs_device));

  // setup the device
  bzero(dev, sizeof(struct devfs_device));
  strncpy((char *)dev->name, name, NAME_MAX);
  // TODO: instead of using the next address, look for an available one
  dev->addr = NULL == head ? 1 : head->addr + 1;
  dev->ops  = ops;
  dev->mode = mode;

  // add device to the list
  slist_add_start(&head, dev, struct devfs_device);

  return dev->addr;
}

int32_t devfs_device_unregister(const char *name) {
  struct devfs_device *dev = devfs_device_from_name(name);

  if (NULL == dev)
    return -EFAULT;

  // remove the device from the device list
  slist_del(&head, dev, struct devfs_device);

  // free the device object
  heap_free(dev);

  return 0;
}
