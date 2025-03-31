#include "core/ps2.h"
#include "core/timer.h"

#include "util/printk.h"
#include "util/string.h"

#include "types.h"
#include "errno.h"

void ps2_first_flush() {
  while (true) {
    if (!ps2_readable())
      timer_sleep(PS2_TIMEOUT_STATUS);

    if (!ps2_readable())
      return;

    in8(PS2_PORT_DATA);
  }
}

int32_t ps2_first_read(uint8_t *data) {
  /*

   * if we directly read from the controller, we will end up
   * reading from the device that's connected to the port one

  */
  return ps2_read(data);
}

int32_t ps2_first_write(uint8_t data) {
  /*

   * similar to __ps2_first_read(), if we directly write to the
   * controller's data port, we'll end up writing to the device
   * connected to port one

  */
  return ps2_write(data);
}

int32_t ps2_first_cmd(uint8_t cmd) {
  int32_t err  = 0;
  uint8_t data = 0;

  // send the command
  if ((err = ps2_first_write(cmd)) != 0)
    return err;

  // get the result
  if ((err = ps2_first_read(&data)) != 0)
    return err;

  // check the response
  switch (data) {
  // if it's an ACK, command was successful
  case PS2_RES_ACK:
    return 0;

  // if the device asked for it, resend the command
  case PS2_RES_RESEND:
    return ps2_first_cmd(cmd);
  }

  // any other response means the command failed
  return -EIO;
}

int32_t ps2_first_enable(uint8_t *id) {
  uint8_t res = 0;
  int32_t err = 0;

  // send test command
  if ((err = ps2_cmd_read(PS2_CMD_TEST_FIRST, &res)) != 0) {
    ps2_debg("failed to send test command to port 1: %s", strerror(err));
    return err;
  }

  // check the test command result
  if (res != PS2_RES_PORT_TEST_PASS) {
    ps2_debg("port 1 test command failed (0x%x)", res);
    return -EFAULT;
  }

  /*

   * modify the configuration, we'll enable the clock for port 1
   * by clearing the port 1 clock bit

   * then we'll disable the translation for port 1 by clearing the
   * translation bit

  */
  if ((err = ps2_conf(0, PS2_CONFIG_FIRST_CLOCK | PS2_CONFIG_FIRST_TRANS)) != 0) {
    ps2_debg("failed change the config byte: %s", strerror(err));
    return err;
  }

  // enable the port
  ps2_cmd(PS2_CMD_ENABLE_FIRST);

  // flush the data port
  ps2_first_flush();

  // disable scanning of the device
  if ((err = ps2_first_cmd(PS2_DEV_CMD_DISABLE_SCAN)) != 0) {
    ps2_debg("disable scan command failed: %s", strerror(err));
    return err;
  }

  // reset the device
  if ((err = ps2_first_cmd(PS2_DEV_CMD_RESET)) != 0) {
    ps2_debg("reset command failed: %s", strerror(err));
    return err;
  }

  // flush the reset command's result
  ps2_first_flush();

  // send the identify command
  if ((err = ps2_first_cmd(PS2_DEV_CMD_IDENTIFY)) != 0) {
    ps2_debg("identify command failed: %s", strerror(err));
    return err;
  }

  // read the device ID
  if ((err = ps2_first_read(&id[0])) != 0)
    return err;
  ps2_first_read(&id[1]);

  return 0;
}

int32_t ps2_first_disable() {
  int32_t err = 0;

  // disable the port
  ps2_cmd(PS2_CMD_DISABLE_FIRST);

  // disable the interrupt
  if ((err = ps2_conf(0, PS2_CONFIG_FIRST_INT)) != 0) {
    ps2_debg("failed change the config byte: %s", strerror(err));
    return err;
  }

  // success
  return 0;
}

int32_t ps2_first_start() {
  int32_t err  = 0;
  uint8_t conf = 0;

  /*

   * to start data transmission we'll need to enable interrupts
   * for this port, which is done by setting the interrupt bit
   * for this port in the configuration byte

  */
  if ((err = ps2_conf(PS2_CONFIG_FIRST_INT, 0)) != 0) {
    ps2_debg("failed change the config byte: %s", strerror(err));
    return err;
  }

  // enable scanning of the device
  if ((err = ps2_first_cmd(PS2_DEV_CMD_ENABLE_SCAN)) != 0) {
    ps2_debg("enable scan command failed: %s", strerror(err));
    return err;
  }

  // flush the data port
  ps2_first_flush();

  // success
  return 0;
}

int32_t ps2_first_stop() {
  int32_t err = 0;

  // flush the data port
  ps2_first_flush();

  // disable scanning of the device
  if ((err = ps2_first_cmd(PS2_DEV_CMD_DISABLE_SCAN)) != 0) {
    ps2_debg("disable scan command failed: %s", strerror(err));
    return err;
  }

  /*

   * since we disabled scanning, we'll no longer receive data from
   * the device, however we should also disable the interrupt and
   * to do so, well need to clear the interrupt bit for port 1

  */
  if ((err = ps2_conf(0, PS2_CONFIG_FIRST_INT)) != 0) {
    ps2_debg("failed change the config byte: %s", strerror(err));
    return err;
  }

  // success
  return 0;
}

// define the first PS2 port structure
ps2_port_t ps2_first_port = {
    .name = "first port",

    .enable  = ps2_first_enable,
    .disable = ps2_first_disable,

    .start = ps2_first_start,
    .stop  = ps2_first_stop,

    .flush = ps2_first_flush,
    .write = ps2_first_write,
    .read  = ps2_first_read,
    .cmd   = ps2_first_cmd,
};
