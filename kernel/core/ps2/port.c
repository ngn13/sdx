#include "core/ps2.h"
#include "core/pic.h"

#include "util/lock.h"
#include "util/printk.h"
#include "util/string.h"

#include "types.h"
#include "errno.h"

#define ps2_port_info(f, ...) pinfo("PS/2: (%s) " f, port->name, ##__VA_ARGS__)
#define ps2_port_fail(f, ...) pfail("PS/2: (%s) " f, port->name, ##__VA_ARGS__)
#define ps2_port_debg(f, ...) pdebg("PS/2: (%s) " f, port->name, ##__VA_ARGS__)

// list of PS/2 ports
ps2_port_t *ps2_ports[] = {
    &ps2_first_port,
    &ps2_second_port,
    NULL,
};

// marco to check if port is a NULL pointer
#define __port_check()                                                                                                 \
  if (NULL == port)                                                                                                    \
  return -EINVAL

ps2_port_t *ps2_port_find(uint8_t *id) {
  if (NULL == id)
    return NULL;

  ps2_port_foreach() {
    // check the first byte of the ID
    if (cur->id[0] != id[0])
      continue;

    // check the second byte of the ID
    if (id[1] == 0 || cur->id[1] == id[1])
      return cur;
  }

  // port not found
  return NULL;
}

int32_t ps2_port_enable(ps2_port_t *port) {
  __port_check();
  int32_t err = 0;

  // enable the port and check the result
  if ((err = port->enable(port)) != 0) {
    ps2_port_debg("failed to enable port: %s", strerror(err));
    port->enabled = false;
    return err;
  }

  // update the port status
  ps2_port_debg("device ID 0x%x,0x%x", port->id[0], port->id[1]);
  port->enabled = true;

  // return the result
  return 0;
}

int32_t ps2_port_disable(ps2_port_t *port) {
  __port_check();
  int32_t err = 0;

  // disable the port and check the result
  if ((err = port->disable(port)) != 0) {
    ps2_port_debg("failed to disable port: %s", strerror(err));
    return err;
  }

  // update the status of the port
  port->enabled = false;
  return 0;
}

int32_t ps2_port_start(ps2_port_t *port) {
  __port_check();
  int32_t err = 0;

  if ((err = port->start(port)) != 0) {
    ps2_port_debg("failed to start port: %s", strerror(err));
    return err;
  }

  // unmask the IRQ
  pic_unmask(port->int_vector);
  return 0;
}

int32_t ps2_port_stop(ps2_port_t *port) {
  __port_check();
  int32_t err = 0;

  if ((err = port->stop(port)) != 0) {
    ps2_port_debg("failed to stop port: %s", strerror(err));
    return err;
  }

  // mask the IRQ
  pic_mask(port->int_vector);
  return 0;
}

int32_t ps2_port_write(ps2_port_t *port, uint8_t data, bool timeout) {
  __port_check();
  return port->write(port, data, timeout);
}

int32_t ps2_port_read(ps2_port_t *port, uint8_t *data, bool timeout) {
  __port_check();
  return port->read(port, data, timeout);
}

int32_t ps2_port_cmd(ps2_port_t *port, uint8_t cmd) {
  __port_check();

  int32_t err = 0;

  // lock port's command lock
  spinlock_acquire(&port->cmd_lock);

  // run the command function of the port
  err = port->cmd(port, cmd);

  spinlock_release();
  return err;
}
