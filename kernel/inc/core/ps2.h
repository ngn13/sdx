#pragma once

#ifndef __ASSEMBLY__
#include "util/printk.h"
#include "types.h"

#define ps2_info(f, ...) pinfo("PS/2: " f, ##__VA_ARGS__)
#define ps2_fail(f, ...) pfail("PS/2: " f, ##__VA_ARGS__)
#define ps2_debg(f, ...) pdebg("PS/2: " f, ##__VA_ARGS__)

// an enum for different ports
typedef enum {
  PS2_PORT_1,
  PS2_PORT_2,
} ps2_port_t;

// used for looping through the ports
#define PS2_PORT_MIN (PS2_PORT_1)
#define PS2_PORT_MAX (PS2_PORT_2)

// IO ports
#define PS2_PORT_DATA    (0x60)
#define PS2_PORT_COMMAND (0x64)
#define PS2_PORT_STATUS  (0x64)

// commands
#define PS2_CMD_READ_0         (0x20)
#define PS2_CMD_WRITE_0        (0x60)
#define PS2_CMD_TEST           (0xAA)
#define PS2_CMD_DISABLE_SECOND (0xA7)
#define PS2_CMD_ENABLE_SECOND  (0xA8)
#define PS2_CMD_TEST_SECOND    (0xA9)
#define PS2_CMD_DISABLE_FIRST  (0xAD)
#define PS2_CMD_ENABLE_FIRST   (0xAE)
#define PS2_CMD_TEST_FIRST     (0xAB)

// status bits
#define PS2_STATUS_OUTPUT (0)
#define PS2_STATUS_INPUT  (1)

// config bits
#define PS2_CONFIG_FIRST_INT    (0)
#define PS2_CONFIG_SECOND_INT   (1)
#define PS2_CONFIG_SYSTEM_FLAG  (2)
#define PS2_CONFIG_FIRST_CLOCK  (4)
#define PS2_CONFIG_SECOND_CLOCK (5)
#define PS2_CONFIG_FIRST_TRANS  (6)

// initialize the PS/2 controller
int32_t ps2_init();

// basic data IO operations
uint8_t ps2_read();
void    ps2_write(uint8_t data);

// send commands and receive responses
uint8_t ps2_cmd(uint8_t cmd);
void    ps2_cmd_with(uint8_t cmd, uint8_t data);

// core/ps2/port.c
int32_t ps2_port_enable(ps2_port_t port);
int32_t ps2_port_disable(ps2_port_t port);

#endif
