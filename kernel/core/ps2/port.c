#include "core/ps2.h"

#include "util/bit.h"
#include "util/io.h"

#include "types.h"
#include "errno.h"
#include <stdint.h>

/*

 * a simple structure to store information about a single port

 * since different ports are accessed in different ways, this
 * structure also contains function pointers for common port
 * operations, so we can use the same function for doing
 * operations on different ports, and we don't have to worry
 * about using the right function

*/
struct port {
  bool    enabled; // is the port enabled
  int32_t dev;     // device address for the port

  // port specific functions
  int32_t (*enable)();
  int32_t (*disable)();
};

int32_t __port_1_enable() {
  uint8_t config = 0, err = 0;

  // send test command and check the response
  if ((err = ps2_cmd(PS2_CMD_TEST_FIRST)) != 0) {
    ps2_debg("test for port 1 failed (0x%x)", err);
    return -EFAULT;
  }

  // configure the port
  config = ps2_cmd(PS2_CMD_READ_0);
  bit_set(config, PS2_CONFIG_FIRST_INT, 1); // enable port 1 interrupt
  ps2_cmd_with(PS2_CMD_WRITE_0, config);

  // enable the port
  ps2_cmd(PS2_CMD_ENABLE_FIRST);

  // TODO: reset
  return -ENOSYS;
}

int32_t __port_1_disable() {
  // disable the port
  ps2_cmd(PS2_CMD_DISABLE_FIRST);

  // reconfigure it
  uint8_t config = ps2_cmd(PS2_CMD_READ_0);
  bit_set(config, PS2_CONFIG_FIRST_INT, 0);   // disable port 1 interrupt
  bit_set(config, PS2_CONFIG_FIRST_CLOCK, 0); // enable port 1 clock
  bit_set(config, PS2_CONFIG_FIRST_TRANS, 0); // disable port 1 translation
  ps2_cmd_with(PS2_CMD_WRITE_0, config);

  return 0;
}

int32_t __port_2_enable() {
  uint8_t config = 0, err = 0;

  // send test command and check the response
  if ((err = ps2_cmd(PS2_CMD_TEST_SECOND)) != 0) {
    ps2_debg("test for port 1 failed (0x%x)", err);
    return -EFAULT;
  }

  // configure the port
  config = ps2_cmd(PS2_CMD_READ_0);
  bit_set(config, PS2_CONFIG_SECOND_INT, 1); // enable port 1 interrupt
  ps2_cmd_with(PS2_CMD_WRITE_0, config);

  // enable the port
  ps2_cmd(PS2_CMD_ENABLE_SECOND);

  // TODO: reset
  return -ENOSYS;
}

int32_t __port_2_disable() {
  // disable the port
  ps2_cmd(PS2_CMD_DISABLE_SECOND);

  // reconfigure it
  uint8_t config = ps2_cmd(PS2_CMD_READ_0);
  bit_set(config, PS2_CONFIG_SECOND_INT, 0);   // disable port 1 interrupt
  bit_set(config, PS2_CONFIG_SECOND_CLOCK, 0); // enable port 1 clock
  ps2_cmd_with(PS2_CMD_WRITE_0, config);

  return 0;
}

// list of ports
struct port ports[] = {
    {.enable = __port_1_enable, .disable = __port_1_disable},
    {.enable = __port_2_enable, .disable = __port_2_disable}
};

#define port_count()   (sizeof(ports) / sizeof(ports[0]))
#define port_status(p) (ports[p].enabled)

int32_t ps2_port_enable(ps2_port_t port) {
  if (port >= port_count())
    return -EINVAL;

  // enable the port and check the result
  int32_t err       = ports[port].enable();
  port_status(port) = err == 0;

  // return the result
  return err;
}

int32_t ps2_port_disable(ps2_port_t port) {
  if (port >= port_count())
    return -EINVAL;

  // disable the port and check the result
  int32_t err       = ports[port].disable();
  port_status(port) = err != 0;

  // return the result
  return err;
}
