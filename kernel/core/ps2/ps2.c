#include "core/ps2.h"
#include "core/acpi.h"

#include "util/printk.h"
#include "util/bit.h"
#include "util/io.h"

#include "errno.h"
#include "types.h"

#define ps2_info(f, ...) pinfo("PS/2: " f, ##__VA_ARGS__)
#define ps2_fail(f, ...) pfail("PS/2: " f, ##__VA_ARGS__)
#define ps2_debg(f, ...) pdebg("PS/2: " f, ##__VA_ARGS__)

#define PS2_PORT_DATA    (0x60)
#define PS2_PORT_COMMAND (0x64)
#define PS2_PORT_STATUS  (0x64)

#define PS2_CMD_READ_0         (0x20)
#define PS2_CMD_WRITE_0        (0x60)
#define PS2_CMD_DISABLE_SECOND (0xA7)
#define PS2_CMD_ENABLE_SECOND  (0xA8)
#define PS2_CMD_TEST_SECOND    (0xA9)
#define PS2_CMD_DISABLE_FIRST  (0xAD)
#define PS2_CMD_ENABLE_FIRST   (0xAE)
#define PS2_CMD_TEST_FIRST     (0xAB)

#define PS2_STATUS_OUTPUT (0)
#define PS2_STATUS_INPUT  (1)

#define PS2_CONFIG_FIRST_INT    (0)
#define PS2_CONFIG_SECOND_INT   (1)
#define PS2_CONFIG_SYSTEM_FLAG  (2)
#define PS2_CONFIG_FIRST_CLOCK  (4)
#define PS2_CONFIG_SECOND_CLOCK (5)
#define PS2_CONFIG_FIRST_TRANS  (6)

int32_t ps2_init() {
  // make sure we have PS/2 controller
  if (!acpi_supports_8042_ps2())
    return -EFAULT;

  uint8_t port_count = 2, port_err = 0;
  uint8_t config = 0;

  // disable both devices
  ps2_cmd(PS2_CMD_DISABLE_FIRST);
  ps2_cmd(PS2_CMD_DISABLE_SECOND);

  // flush the data buffer
  in8(PS2_PORT_DATA);

  // configure port 1
  config = ps2_cmd(PS2_CMD_READ_0);
  bit_set(config, PS2_CONFIG_FIRST_INT, 0);   // disable port 1 interrupt
  bit_set(config, PS2_CONFIG_FIRST_CLOCK, 0); // enable port 1 clock
  bit_set(config, PS2_CONFIG_FIRST_TRANS, 0); // disable port 1 translation
  ps2_cmd_with(PS2_CMD_WRITE_0, config);

  // test port 1
  if ((port_err = ps2_cmd(PS2_CMD_TEST_FIRST)) != 0){
    ps2_debg("test for port 1 failed (0x%x)", port_err);
    port_count--;
  }

  // check if port 2 is available
  ps2_cmd(PS2_CMD_ENABLE_SECOND);
  config = ps2_cmd(PS2_CMD_READ_0);

  if (!bit_get(config, PS2_CONFIG_SECOND_CLOCK)) {
    // if so, configure port 2
    ps2_cmd(PS2_CMD_DISABLE_SECOND);
    bit_set(config, PS2_CONFIG_SECOND_INT, 0);   // disable port 2 interrupt
    bit_set(config, PS2_CONFIG_SECOND_CLOCK, 0); // enable port 2 clock
    ps2_cmd_with(PS2_CMD_WRITE_0, config);

    // also test it
    if ((port_err = ps2_cmd(PS2_CMD_TEST_SECOND)) != 0){
      ps2_debg("test for port 2 failed (0x%x)", port_err);
      port_count--;
    }
  }

  // check if tests for both of the ports failed
  if (!port_count) {
    ps2_fail("no available ports");
    return 0;
  }

  ps2_info("successfully tested %u ports", port_count);

  // TODO: enable, reset and detect devices

  return -ENOSYS;
}

uint8_t ps2_read() {
  // wait until data can be read (output buffer is full)
  while (!bit_get(in8(PS2_PORT_STATUS), PS2_STATUS_OUTPUT))
    continue;

  // read the data
  return in8(PS2_PORT_DATA);
}

void ps2_write(uint8_t data) {
  // wait until data can be written (input buffer is clear)
  while (bit_get(in8(PS2_PORT_STATUS), PS2_STATUS_INPUT))
    continue;

  // read the data
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
