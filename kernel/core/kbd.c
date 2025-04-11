#include "core/kbd.h"
#include "core/driver.h"
#include "core/ps2.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/mem.h"

#include "fs/devfs.h"
#include "fs/fs.h"

#include "types.h"
#include "errno.h"

#define kbd_info(f, ...) pinfo("Keyboard: " f, ##__VA_ARGS__)
#define kbd_fail(f, ...) pfail("Keyboard: " f, ##__VA_ARGS__)
#define kbd_debg(f, ...) pdebg("Keyboard: " f, ##__VA_ARGS__)

#define KBD_DEV_NAME  "keyboard"
#define KBD_DEV_MAJOR (10)
#define KBD_BUF_SIZE  (UINT8_MAX)

driver_new(kbd, kbd_load, kbd_unload, &ps2_driver, );

ps2_port_t  *__kbd_port = NULL;
char         __kbd_buffer[KBD_BUF_SIZE];
ps2_dev_id_t __kbd_ids[] = {
    // MF2 keyboard IDs
    {0xAB, 0x83},
    {0xAB, 0x41},
    {0xAB, 0xC1},
};

int64_t kbd_dev_read(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  // TODO: implement
  return 0;
}

int64_t kbd_dev_write(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  // you can only read from the keyboard device
  return -ENOSYS;
}

// keyboard device operations
devfs_ops_t kbd_ops = {
    .open  = fs_default,
    .close = fs_default,
    .read  = kbd_dev_read,
    .write = kbd_dev_write,
};

int32_t kbd_load() {
  int32_t err = 0;
  uint8_t i   = 0;

  // look for a PS/2 keyboard
  for (; i < sizeof(__kbd_ids) / sizeof(__kbd_ids[0]); i++)
    if (NULL != (__kbd_port = ps2_port_find(__kbd_ids[i])))
      break;

  // check we found a PS/2 keyboard
  if (NULL == __kbd_port)
    return -EFAULT;

  // clear the keyboard buffer
  bzero(__kbd_buffer, sizeof(__kbd_buffer));

  // enable the PS/2 keyboard
  if ((err = ps2_port_start(__kbd_port)) != 0) {
    kbd_debg("failed to enable the PS/2 keyboard: %s", strerror(err));
    return err;
  }

  // register the keyboard device
  if ((err = devfs_register(KBD_DEV_MAJOR, KBD_DEV_NAME, &kbd_ops)) != 0) {
    kbd_fail("failed to register the keyboard device: %s", strerror(err));
    ps2_port_disable(__kbd_port);
    return err;
  }

  // create the keyboard device
  if ((err = devfs_create(devfs_addr(KBD_DEV_MAJOR, 0), NULL, MODE_USRR)) != 0) {
    kbd_fail("failed to create the keyboard device: %s", strerror(err));
    devfs_unregister(KBD_DEV_MAJOR);
    ps2_port_disable(__kbd_port);
  }

  // success
  return 0;
}

int32_t kbd_unload() {
  int32_t err = 0;

  // unregister the keyboard device
  if ((err = devfs_unregister(KBD_DEV_MAJOR)) != 0) {
    kbd_fail("failed to unregister keyboard device: %s", strerror(err));
    return err;
  }

  // disable the PS/2 keyboard
  if (NULL != __kbd_port)
    ps2_port_stop(__kbd_port);

  // success
  return 0;
}
