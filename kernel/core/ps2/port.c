#include "core/ps2.h"
#include "core/timer.h"

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

int32_t ps2_port_enable(ps2_port_t *port) {
  if (NULL == port)
    return -EINVAL;

  int32_t err = 0;

  // enable the port and check the result
  if ((err = port->enable(port->id)) != 0) {
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
  if (NULL == port)
    return -EINVAL;

  int32_t err = 0;

  // disable the port and check the result
  if ((err = port->disable()) != 0) {
    ps2_port_debg("failed to disable port: %s", strerror(err));
    return err;
  }

  // update the status of the port
  port->enabled = false;
  return 0;
}
