#include "core/ps2.h"
#include "core/acpi.h"
#include "core/driver.h"
#include "core/im.h"
#include "core/timer.h"

#include "util/string.h"
#include "util/lock.h"
#include "util/mem.h"
#include "util/io.h"

#include "errno.h"
#include "types.h"

driver_new(ps2, ps2_load, NULL, &acpi_driver, );
spinlock_t __ps2_conf_lock;

int32_t ps2_load() {
  // make sure we have a PS/2 controller
  if (!acpi_supports_8042_ps2()) {
    ps2_fail("no available controller");
    return -EFAULT;
  }

  uint8_t res = 0, count = 0;
  int32_t err = 0;

  /*

   * clear data structure of all ports, disable them
   * and register the interrupt handler they are using

  */
  ps2_port_foreach() {
    // clear the data structure
    cur->enabled = false;
    bzero(cur->id, sizeof(cur->id));

    // disable the port
    ps2_port_disable(cur);

    // register interrupt handler
    im_add_handler(cur->int_vector, cur->int_handler);
  }

  // save the config (in res)
  if ((err = ps2_conf_read(&res)) != 0) {
    ps2_fail("failed to read the config byte: %s", strerror(err));
    return err;
  }

  // test the controller
  if ((err = ps2_cmd_read(PS2_CMD_TEST, &res)) != 0) {
    ps2_fail("controller test command failed: %s", strerror(err));
    return err;
  }

  // check the test result
  if (res != PS2_RES_TEST_PASS) {
    ps2_fail("controller test failed");
    return -EFAULT;
  }

  // restore the config (saved in res)
  if ((err = ps2_conf_write(res)) != 0) {
    ps2_fail("failed to write the config byte: %s", strerror(err));
    return err;
  }

  // enable all ports
  ps2_port_foreach() {
    if (ps2_port_enable(cur) == 0)
      count++;
  }

  // check if we have successfuly enabled any ports
  if (count == 0) {
    ps2_fail("no available PS/2 ports");
    return -EFAULT;
  }

  ps2_info("successfully initialized %u ports", count);

  // initialize the PS/2 config R/W lock
  spinlock_init(&__ps2_conf_lock);

  return 0;
}

int32_t ps2_read(uint8_t *data) {
  if (NULL == data)
    in8(PS2_PORT_DATA);
  else
    *data = in8(PS2_PORT_DATA);
  return 0;
}

int32_t ps2_write(uint8_t data) {
  out8(PS2_PORT_DATA, data);
  return 0;
}

int32_t ps2_conf(uint8_t set, uint8_t clear) {
  uint8_t conf = 0;
  int32_t err  = 0;

  // lock before modifying the config byte
  spinlock_acquire(&__ps2_conf_lock);

  if ((err = ps2_conf_read(&conf)) != 0)
    goto end;

  conf |= set;
  conf &= ~(clear);

  err = ps2_conf_write(conf);
end:
  spinlock_release();
  return err;
}

int32_t ps2_cmd(uint8_t cmd) {
  out8(PS2_PORT_COMMAND, cmd);
  return 0;
}

int32_t ps2_cmd_write(uint8_t cmd, uint8_t data) {
  out8(PS2_PORT_COMMAND, cmd);

  if (!ps2_writeable())
    timer_sleep(PS2_TIMEOUT_CMD);

  if (!ps2_writeable())
    return -ETIME;

  return ps2_write(data);
}

int32_t ps2_cmd_read(uint8_t cmd, uint8_t *data) {
  out8(PS2_PORT_COMMAND, cmd);

  if (!ps2_readable())
    timer_sleep(PS2_TIMEOUT_CMD);

  if (!ps2_readable())
    return -ETIME;

  return ps2_read(data);
}
