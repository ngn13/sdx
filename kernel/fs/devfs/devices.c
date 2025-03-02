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

struct devfs_device *devfs_device_find(const char *name, uint64_t *index) {
  if (NULL == name)
    return NULL;

  // reset the index
  if (NULL != index)
    *index = 0;

  // search the device list
  slist_foreach(&head, struct devfs_device) {
    // check the name
    if (streq((char *)cur->name, (char *)name))
      return cur;

    // increase the index
    if (NULL != index)
      (*index)++;
  }

  return NULL;
}

struct devfs_device *devfs_device_at(uint64_t index) {
  slist_foreach(&head, struct devfs_device) {
    if (index <= 0)
      return cur;
    index--;
  }

  return NULL;
}

int32_t devfs_device_register(const char *name, devfs_ops_t *ops, mode_t mode) {
  if (NULL == name || NULL == ops)
    return -EINVAL;

  struct devfs_device *dev = heap_alloc(sizeof(struct devfs_device));

  // setup the device
  bzero(dev, sizeof(struct devfs_device));
  strncpy((char *)dev->name, name, NAME_MAX);
  dev->ops  = ops;
  dev->mode = mode;

  // add device to the list
  slist_add_start(&head, dev, struct devfs_device);

  return 0;
}

int32_t devfs_device_unregister(const char *name) {
  struct devfs_device *dev = devfs_device_find(name, NULL);

  if (NULL == dev)
    return -EFAULT;

  // remove the device from the device list
  slist_del(&head, dev, struct devfs_device);

  // free the device object
  heap_free(dev);

  return 0;
}
