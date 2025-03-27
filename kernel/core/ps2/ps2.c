#include "core/ps2.h"
#include "core/acpi.h"

#include "util/bit.h"
#include "util/io.h"

#include "errno.h"
#include "types.h"

int32_t ps2_init() {
  // make sure we have a PS/2 controller
  if (!acpi_supports_8042_ps2()) {
    ps2_fail("no available controller");
    return -EFAULT;
  }

  ps2_port_t port = 0;
  uint8_t    temp = 0;

  // disable all ports
  for (port = 0; port <= PS2_PORT_MAX; port++)
    ps2_port_disable(port);

  // flush the data buffer
  in8(PS2_PORT_DATA);

  // save the config
  temp = ps2_cmd(PS2_CMD_READ_0);

  // test the controller
  if (ps2_cmd(PS2_CMD_TEST) != 0x55) {
    ps2_fail("controller test failed");
    return -EFAULT;
  }

  // restore the config
  ps2_cmd_with(PS2_CMD_WRITE_0, temp);
  temp = 0;

  // enable all ports
  for (port = 0; port <= PS2_PORT_MAX; port++)
    if (ps2_port_enable(port) == 0)
      temp++;

  // check if we have successfuly enabled any ports
  if (temp == 0) {
    ps2_fail("no available PS/2 ports");
    return -EFAULT;
  }

  ps2_info("successfully initialized %u ports", temp);
  return 0;
}

uint8_t ps2_read() {
  while (!bit_get(in8(PS2_PORT_STATUS), PS2_STATUS_OUTPUT))
    continue;
  return in8(PS2_PORT_DATA);
}

void ps2_write(uint8_t data) {
  while (bit_get(in8(PS2_PORT_STATUS), PS2_STATUS_INPUT))
    continue;
  out8(PS2_PORT_DATA, data);
}

uint8_t ps2_cmd(uint8_t cmd) {
  out8(PS2_PORT_COMMAND, cmd);

  switch (cmd) {
  case PS2_CMD_DISABLE_SECOND:
  case PS2_CMD_ENABLE_SECOND:
  case PS2_CMD_DISABLE_FIRST:
  case PS2_CMD_ENABLE_FIRST:
  case PS2_CMD_WRITE_0:
    return 0;
  }

  return ps2_read();
}

void ps2_cmd_with(uint8_t cmd, uint8_t data) {
  out8(PS2_PORT_COMMAND, cmd);
  ps2_write(data);
}
