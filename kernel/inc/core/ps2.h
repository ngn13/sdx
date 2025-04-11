#pragma once

#ifndef __ASSEMBLY__
#include "core/driver.h"
#include "core/im.h"

#include "util/printk.h"
#include "util/lock.h"
#include "util/io.h"

#include "types.h"

#define ps2_info(f, ...) pinfo("PS/2: " f, ##__VA_ARGS__)
#define ps2_fail(f, ...) pfail("PS/2: " f, ##__VA_ARGS__)
#define ps2_debg(f, ...) pdebg("PS/2: " f, ##__VA_ARGS__)

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

// device commands
#define PS2_DEV_CMD_IDENTIFY     (0xF2)
#define PS2_DEV_CMD_ENABLE_SCAN  (0xF4)
#define PS2_DEV_CMD_DISABLE_SCAN (0xF5)
#define PS2_DEV_CMD_RESET        (0xFF)

// results
#define PS2_RES_TEST_PASS      (0x55)
#define PS2_RES_PORT_TEST_PASS (0x00)
#define PS2_RES_ACK            (0xFA)
#define PS2_RES_RESEND         (0xFE)

// status bits
#define PS2_STATUS_OUTPUT (1 << 0)
#define PS2_STATUS_INPUT  (1 << 1)

// config bits
#define PS2_CONFIG_FIRST_INT    (1 << 0)
#define PS2_CONFIG_SECOND_INT   (1 << 1)
#define PS2_CONFIG_SYSTEM_FLAG  (1 << 2)
#define PS2_CONFIG_FIRST_CLOCK  (1 << 4)
#define PS2_CONFIG_SECOND_CLOCK (1 << 5)
#define PS2_CONFIG_FIRST_TRANS  (1 << 6)

// timeouts
#define PS2_TIMEOUT_CMD   (100)      // PS/2 command R/W timeout (100ms)
#define PS2_TIMEOUT_FLUSH (100)      // port input buffer flush timeout (100ms)
#define PS2_TIMEOUT_READ  (1000 * 2) // (default) port read timeout (2s)
#define PS2_TIMEOUT_WRITE (1000)     // (default) port write timeout (1s)

driver_extern(ps2);

// define a device ID type
typedef uint8_t ps2_dev_id_t[2];

/*

 * a simple structure to store information about a single port

 * since different ports are accessed in different ways, this
 * structure also contains function pointers for common port
 * operations, so we can use the same function for doing
 * operations on different ports, and we don't have to worry
 * about using the right function

*/
typedef struct ps2_port {
  ps2_dev_id_t id;      // device ID
  const char  *name;    // name of the port
  bool         enabled; // is the port enabled

  char    buf[UINT8_MAX - 1]; // data buffer
  uint8_t buf_indx;           // data buffer index

  uint8_t            int_vector;  // interrupt vector for the port
  im_handler_func_t *int_handler; // handler for port's interrupt

  spinlock_t cmd_lock; // command lock
  spinlock_t buf_lock; // data buffer lock

  // port specific functions
  int32_t (*enable)(struct ps2_port *port);  // enable the port
  int32_t (*disable)(struct ps2_port *port); // disable the port

  int32_t (*start)(struct ps2_port *port); // start data transmission
  int32_t (*stop)(struct ps2_port *port);  // stop data transmission

  void (*flush)(struct ps2_port *port);                                // clear data buffer
  int32_t (*write)(struct ps2_port *port, uint8_t data, bool timeout); // write data
  int32_t (*read)(struct ps2_port *port, uint8_t *data, bool timeout); // read data
  int32_t (*cmd)(struct ps2_port *port, uint8_t cmd);                  // send command
} ps2_port_t;

// indiviual port definitions (core/ps2/ports/...)
extern ps2_port_t ps2_first_port;
extern ps2_port_t ps2_second_port;

// list of ports (core/ps2/port.c)
extern ps2_port_t *ps2_ports[];

// used for looping through the ports
#define ps2_port_foreach()                                                                                             \
  for (ps2_port_t *cur = ps2_ports[0], *i = 0; cur != NULL; i = (void *)(uint64_t)i + 1, cur = ps2_ports[(uint64_t)i])

// used for looping through the command queue
#define ps2_cmd_foreach(port)                                                                                          \
  for (ps2_cmd_t *cur = &port->queue[0]; cur != &port->queue[PS2_PORT_QUEUE_SIZE - 1]; cur++)

// initialize the PS/2 controller
int32_t ps2_load();

// basic data IO operations
#define ps2_writeable() (!(in8(PS2_PORT_STATUS) & PS2_STATUS_INPUT))
#define ps2_readable()  (in8(PS2_PORT_STATUS) & PS2_STATUS_OUTPUT)
int32_t ps2_write(uint8_t data); // WARN: check ps2_writeable() before using
int32_t ps2_read(uint8_t *data); // WARN: check ps2_readable() before using

// command functions
int32_t ps2_cmd(uint8_t cmd);
int32_t ps2_cmd_write(uint8_t cmd, uint8_t data);
int32_t ps2_cmd_read(uint8_t cmd, uint8_t *data);

// config functions & macros
#define ps2_conf_write(conf) ps2_cmd_write(PS2_CMD_WRITE_0, conf)
#define ps2_conf_read(conf)  ps2_cmd_read(PS2_CMD_READ_0, conf)
int32_t ps2_conf(uint8_t set, uint8_t clear);

// core/ps2/port.c
ps2_port_t *ps2_port_find(uint8_t *id);

#define ps2_port_buf_is_full(port)  (port->buf_indx >= sizeof(port->buf))
#define ps2_port_buf_is_empty(port) (port->buf_indx <= 0)
#define ps2_port_buf_write(port, c) (port->buf[port->buf_indx++] = c)
#define ps2_port_buf_read(port)     (port->buf[--port->buf_indx])

int32_t ps2_port_enable(ps2_port_t *port);
int32_t ps2_port_disable(ps2_port_t *port);

int32_t ps2_port_start(ps2_port_t *port);
int32_t ps2_port_stop(ps2_port_t *port);

#define ps2_port_flush(port) port->flush(port)
int32_t ps2_port_write(ps2_port_t *port, uint8_t data, bool timeout);
int32_t ps2_port_read(ps2_port_t *port, uint8_t *data, bool timeout);
int32_t ps2_port_cmd(ps2_port_t *port, uint8_t cmd);

#endif
