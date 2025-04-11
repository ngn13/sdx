#include "core/pic.h"
#include "core/ps2.h"
#include "core/timer.h"

#include "sched/sched.h"
#include "sched/task.h"
#include "util/printk.h"
#include "util/string.h"

#include "types.h"
#include "errno.h"

void ps2_first_flush(ps2_port_t *port) {
  while (true) {
    // sleep if not readable
    if (!ps2_readable())
      timer_sleep(PS2_TIMEOUT_FLUSH);

    // if still not readable, flush is complete
    if (!ps2_readable())
      return;

    ps2_read(NULL);
  }
}

int32_t ps2_first_read(ps2_port_t *port, uint8_t *data, bool timeout) {
  if (timeout)
    sched_block_timeout(TASK_BLOCK_INPUT, PS2_TIMEOUT_READ, !ps2_readable());
  else
    sched_block_until(TASK_BLOCK_INPUT, !ps2_readable());

  if (!ps2_readable())
    return -ETIME;

  // when we directly read from data port, we read from the first port
  return ps2_read(data);
}

int32_t ps2_first_write(ps2_port_t *port, uint8_t data, bool timeout) {
  do {
    if (ps2_writeable())
      break;
    timer_sleep(PS2_TIMEOUT_WRITE);
  } while (!timeout);

  if (!ps2_writeable())
    return -ETIME;

  // when we directly write to data port, it goes to the first port
  return ps2_write(data);
}

int32_t ps2_first_cmd(ps2_port_t *port, uint8_t cmd) {
  int32_t err  = 0;
  uint8_t data = 0;

  // send the command
  if ((err = ps2_port_write(port, cmd, true)) != 0)
    return err;

  // get & check the response
  while ((err = ps2_port_read(port, &data, true)) == 0) {
    switch (data) {
    // if it's an ACK, command was successful
    case PS2_RES_ACK:
      return 0;

    // if the device asked for it, resend the command
    case PS2_RES_RESEND:
      return ps2_port_cmd(port, cmd);

    // otherwise data we read is not a response, save it
    default:
      if (!ps2_port_buf_is_full(port))
        ps2_port_buf_write(port, data);
      else
        return -EIO;
    }
  }

  // if read fails, return the error
  return err;
}

int32_t ps2_first_enable(ps2_port_t *port) {
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
  ps2_port_flush(port);

  // disable scanning of the device
  if ((err = ps2_port_cmd(port, PS2_DEV_CMD_DISABLE_SCAN)) != 0) {
    ps2_debg("disable scan command failed: %s", strerror(err));
    return err;
  }

  // reset the device
  if ((err = ps2_port_cmd(port, PS2_DEV_CMD_RESET)) != 0) {
    ps2_debg("reset command failed: %s", strerror(err));
    return err;
  }

  // flush the reset command's result
  ps2_port_flush(port);

  // send the identify command
  if ((err = ps2_port_cmd(port, PS2_DEV_CMD_IDENTIFY)) != 0) {
    ps2_debg("identify command failed: %s", strerror(err));
    return err;
  }

  // read the device ID
  if ((err = ps2_port_read(port, &port->id[0], true)) != 0)
    return err;
  ps2_port_read(port, &port->id[1], true);

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

int32_t ps2_first_start(ps2_port_t *port) {
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
  if ((err = ps2_port_cmd(port, PS2_DEV_CMD_ENABLE_SCAN)) != 0) {
    ps2_debg("enable scan command failed: %s", strerror(err));
    return err;
  }

  // flush the data port
  ps2_port_flush(port);

  // success
  return 0;
}

int32_t ps2_first_stop(ps2_port_t *port) {
  int32_t err = 0;

  // flush the data port
  ps2_port_flush(port);

  // disable scanning of the device
  if ((err = ps2_port_cmd(port, PS2_DEV_CMD_DISABLE_SCAN)) != 0) {
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

// IRQ handler for the interrupts generated for this port
void ps2_first_irq_handler(im_stack_t *stack) {
  // unblock all the tasks waiting on input
  sched_unblock(NULL, TASK_BLOCK_INPUT);
}

// define the first PS2 port structure
ps2_port_t ps2_first_port = {
    .name = "port 1",

    .int_vector  = pic_to_int(PIC_IRQ_PS2_FIRST),
    .int_handler = ps2_first_irq_handler,

    .enable  = ps2_first_enable,
    .disable = ps2_first_disable,

    .start = ps2_first_start,
    .stop  = ps2_first_stop,

    .flush = ps2_first_flush,
    .write = ps2_first_write,
    .read  = ps2_first_read,
    .cmd   = ps2_first_cmd,
};
