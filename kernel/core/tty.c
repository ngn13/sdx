#include "core/serial.h"
#include "core/tty.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/list.h"
#include "util/lock.h"
#include "util/mem.h"

#include "fs/fs.h"
#include "fs/devfs.h"

#include "mm/heap.h"

#include "limits.h"
#include "types.h"
#include "errno.h"

#define tty_debg(f, ...) pdebg("TTY: " f, ##__VA_ARGS__)
#define tty_info(f, ...) pinfo("TTY: " f, ##__VA_ARGS__)
#define tty_fail(f, ...) pfail("TTY: " f, ##__VA_ARGS__)

tty_t      *__tty_head      = NULL; // first TTY device in the list
const char *__tty_devices[] = {
    "tty0", "tty1", "tty2", "tty3", "tty4", "tty5", "tty6", "tty7", "tty8", "tty9", NULL}; // default TTY device names

struct tty *__tty_find_by_inode(fs_inode_t *inode) {
  slist_foreach(&__tty_head, struct tty) if (cur->dev == inode->addr) return cur;
  return NULL;
}

struct tty *__tty_find_by_name(char *name) {
  slist_foreach(&__tty_head, struct tty) if (streq(cur->name, name)) return cur;
  return NULL;
}

#define __tty_find_and_check()                                                                                         \
  struct tty *tty = __tty_find_by_inode(inode);                                                                        \
  if (NULL == tty)                                                                                                     \
    return -EFAULT;

int32_t __tty_open(fs_inode_t *inode) {
  __tty_find_and_check();
  return tty->ops->open(tty);
}

int32_t __tty_close(fs_inode_t *inode) {
  __tty_find_and_check();
  return tty->ops->close(tty);
}

int64_t __tty_read(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buf) {
  __tty_find_and_check();
  int64_t ret = 0;

  spinlock_acquire(&tty->lock);
  ret = tty->ops->read(tty, offset, size, buf);
  spinlock_release(&tty->lock);

  return ret;
}

int64_t __tty_write(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buf) {
  __tty_find_and_check();
  int64_t ret = 0;

  spinlock_acquire(&tty->lock);
  ret = tty->ops->write(tty, offset, size, buf);
  spinlock_release(&tty->lock);

  return ret;
}

// TTY devfs device operations
devfs_ops_t __tty_ops = {
    .open  = __tty_open,
    .close = __tty_close,
    .read  = __tty_read,
    .write = __tty_write,
};

int32_t __tty_lookup_name(char *name) {
  // check the arguments
  if (NULL == name)
    return -EINVAL;

  char    suffix[3]; // stores the TTY suffix for the current number
  uint8_t num = 0;   // stores the TTY number

  /*

   * this code assumes provided name buffer is >= NAME_MAX
   * since this is called from tty_register() with tty->name
   * this should not an issue, at least for now

  */

  // setup the name buffer
  bzero(name, NAME_MAX);
  name = memcpy(name, "tty", 3);

  for (; num < UINT8_MAX - 1; num++) {
    // create & copy suffix to the end of the name
    itou(num, suffix);
    memcpy(name, suffix, sizeof(suffix));

    // check if a TTY with the name exists
    if (NULL == __tty_find_by_name(name))
      return 0;
  }

  // no available name found
  return -ERANGE;
}

tty_t *tty_register(tty_ops_t *ops, char *name) {
  // check the arguments
  if (NULL == ops)
    return NULL;

  tty_t  *tty = heap_alloc(sizeof(tty_t));
  int32_t err = 0;

  // check if the allocation was successful
  if (NULL == tty) {
    tty_fail("failed to allocate memory for a new TTY deivce");
    return NULL;
  }

  // setup the device
  bzero(tty, sizeof(tty_t));
  spinlock_init(&tty->lock);
  tty->ops = ops;

  if (NULL == name)
    // look for an available name
    err = __tty_lookup_name(name);
  else
    // copy the provided name
    strncpy(tty->name, name, NAME_MAX);

  // check if __tty_find_name() failed
  if (err != 0) {
    tty_fail("failed to get find an available name: %s", strerror(err));
    heap_free(tty);
    return NULL;
  }

  // create a devfs device for the TTY device
  if ((tty->dev = devfs_device_register(tty->name, &__tty_ops, MODE_USRR | MODE_USRW)) < 0) {
    tty_fail("failed to register a devfs device for %s", tty->name);
    heap_free(tty);
    return NULL;
  }

  // add device to the list
  slist_add_start(&__tty_head, tty, tty_t);

  // success
  tty_debg("created a new TTY device");
  pdebg("     |- Name: %s", tty->name);
  pdebg("     |- Device: %d", tty->dev);
  pdebg("     `- Address: 0x%p", tty);
  return 0;
}

int32_t tty_unregister(char *name) {
  // TODO: implement
  return -ENOSYS;
}
