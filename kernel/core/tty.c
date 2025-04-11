#include "core/tty.h"
#include "mm/heap.h"

#include "util/printk.h"
#include "util/mem.h"
#include "util/list.h"
#include "util/lock.h"

#include "fs/fs.h"
#include "fs/devfs.h"

#include "limits.h"
#include "types.h"
#include "errno.h"

#define tty_debg(f, ...) pdebg("TTY: " f, ##__VA_ARGS__)
#define tty_info(f, ...) pinfo("TTY: " f, ##__VA_ARGS__)
#define tty_fail(f, ...) pfail("TTY: " f, ##__VA_ARGS__)

#define TTY_DEV_NAME  "tty"
#define TTY_DEV_MAJOR (4)

tty_t *__tty_head = NULL; // first TTY device in the list

struct tty *__tty_find_by_inode(fs_inode_t *inode) {
  slist_foreach(&__tty_head, struct tty) if (cur->minor == devfs_minor(inode->addr)) return cur;
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
  spinlock_release();

  return ret;
}

int64_t __tty_write(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buf) {
  __tty_find_and_check();
  int64_t ret = 0;

  spinlock_acquire(&tty->lock);
  ret = tty->ops->write(tty, offset, size, buf);
  spinlock_release();

  return ret;
}

// TTY devfs device operations
devfs_ops_t __tty_ops = {
    .open  = __tty_open,
    .close = __tty_close,
    .read  = __tty_read,
    .write = __tty_write,
};

tty_t *tty_register(tty_ops_t *ops) {
  // check the arguments
  if (NULL == ops)
    return NULL;

  tty_t  *tty = heap_alloc(sizeof(tty_t));
  int32_t err = 0;

  // check if the allocation was successful
  if (NULL == tty) {
    tty_fail("failed to allocate memory for a new TTY device");
    return NULL;
  }

  // setup the device
  bzero(tty, sizeof(tty_t));
  spinlock_init(&tty->lock);
  tty->ops = ops;

  if (NULL == __tty_head)
    tty->minor = 0;
  else
    tty->minor = ++__tty_head->minor;

  // create a devfs device for the TTY device
  if ((err = devfs_create(devfs_addr(TTY_DEV_MAJOR, tty->minor), NULL, MODE_USRR | MODE_USRW)) < 0) {
    tty_fail("failed to register a devfs device");
    heap_free(tty);
    return NULL;
  }

  // add device to the list
  slist_add_start(&__tty_head, tty, tty_t);

  // success
  tty_debg("created a new TTY device");
  pdebg("     |- Minor: %d", tty->minor);
  pdebg("     `- Address: 0x%p", tty);
  return 0;
}

int32_t tty_unregister(char *name) {
  // TODO: implement
  return -ENOSYS;
}

int32_t tty_load() {
  // TODO: implement
  return -ENOSYS;
}

int32_t tty_unload() {
  // TODO: implement
  return -ENOSYS;
}
