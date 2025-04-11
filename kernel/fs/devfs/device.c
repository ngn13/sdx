#include "fs/devfs.h"
#include "mm/heap.h"

#include "util/mem.h"
#include "util/string.h"
#include "util/list.h"

#include "limits.h"
#include "errno.h"
#include "types.h"

#define DEVFS_GROUP_MAX      (UINT8_MAX)
#define DEVFS_GROUP_NAME_MAX (NAME_MAX - 2)

struct devfs_group *groups[DEVFS_GROUP_MAX] = {};
#define devfs_group(major) (groups[major - 1])

int32_t devfs_register(uint8_t major, const char *name, devfs_ops_t *ops) {
  if (NULL == name || NULL == ops || major == 0)
    return -EINVAL;

  // major should be unique
  if (NULL != devfs_group(major))
    return -EINVAL;

  // allocate and setup the new group
  struct devfs_group *group = heap_alloc(sizeof(struct devfs_group));

  if (NULL == group) {
    devfs_fail("failed to allocate a new group");
    return -ENOMEM;
  }

  bzero(group, sizeof(struct devfs_group));
  strncpy((void *)group->name, name, NAME_MAX - 2);
  group->ops = ops;

  // add group to the list
  devfs_group(major) = group;

  return 0;
}

int32_t devfs_unregister(uint8_t major) {
  if (major == 0)
    return -EINVAL;

  struct devfs_group  *group = devfs_group(major);
  struct devfs_device *cur = NULL, *pre = NULL;

  if (NULL == group)
    return -EINVAL;

  // destroy all the devices
  for (cur = group->head; cur != NULL;) {
    pre = cur;
    cur = cur->next;
    heap_free(pre);
  }

  // remove group from the list
  devfs_group(major) = NULL;
  heap_free(group);

  return 0;
}

int32_t devfs_create(devfs_addr_t addr, const char *name, mode_t mode) {
  struct devfs_group  *group = devfs_group(devfs_major(addr));
  struct devfs_device *dev   = NULL;

  if (NULL == group)
    return -EINVAL;

  // allocate & setup a new group
  if (NULL == (dev = heap_alloc(sizeof(struct devfs_device)))) {
    devfs_fail("failed to allocate a new device");
    return -ENOMEM;
  }

  bzero(dev, sizeof(struct devfs_device));
  dev->mode = mode;
  dev->addr = addr;

  // if a name is specified, copy that name
  if (NULL != name)
    strncpy((void *)dev->name, name, NAME_MAX);

  // if no name is specified, use the default group name
  else {
    char *end = strncpy((void *)dev->name, group->name, DEVFS_GROUP_MAX);
    // if minor > 0, add minor(-1) to the end of the group name
    if (devfs_minor(addr) > 0)
      itoh(devfs_minor(addr) - 1, end);
  }

  // add device to the group
  slist_add_start(&group->head, dev, struct devfs_device);

  return 0;
}

int32_t devfs_destroy(devfs_addr_t addr) {
  struct devfs_group  *group = devfs_group(devfs_major(addr));
  struct devfs_device *dev   = NULL;

  if (NULL == group)
    return -EINVAL;

  // try to find the device
  slist_foreach(&group->head, struct devfs_device) {
    if (cur->addr == addr) {
      dev = cur;
      break;
    }
  }

  if (NULL == dev)
    return -EINVAL;

  // remove the device from group's list & free
  slist_del(&group->head, dev, struct devfs_device);
  heap_free(dev);

  return 0;
}

struct devfs_group *devfs_get_group(devfs_addr_t addr) {
  return devfs_group(devfs_major(addr));
}

struct devfs_device *devfs_get_device(devfs_addr_t addr, char *name) {
  if (NULL != name) {
    struct devfs_device *dev = NULL;

    while (NULL != (dev = devfs_next_device(dev)))
      if (streq((char *)dev->name, name))
        return dev;
  }

  struct devfs_group *group = devfs_group(devfs_major(addr));

  if (NULL == group)
    return NULL;

  // try to find the device
  slist_foreach(&group->head, struct devfs_device) {
    if (cur->addr == addr)
      return cur;
  }

  return NULL;
}

struct devfs_device *devfs_next_device(struct devfs_device *dev) {
  if (NULL != dev && NULL != dev->next)
    return dev->next;

  for (uint16_t i = NULL == dev ? 0 : devfs_major(dev->addr); i < DEVFS_GROUP_MAX; i++)
    if (NULL != groups[i] && NULL != (dev = groups[i]->head))
      return dev;

  return NULL;
}
