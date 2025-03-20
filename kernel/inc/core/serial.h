#pragma once

#ifndef __ASSEMBLY__
#include <types.h>
#include "fs/devfs.h"

typedef enum {
  SERIAL_PORT_COM1 = 0x3F8,
  SERIAL_PORT_COM2 = 0x2F8,
  SERIAL_PORT_COM3 = 0x3E8,
  SERIAL_PORT_COM4 = 0x2E8,
  SERIAL_PORT_COM5 = 0x5F8,
  SERIAL_PORT_COM6 = 0x4F8,
  SERIAL_PORT_COM7 = 0x5E8,
  SERIAL_PORT_COM8 = 0x4E8,
} serial_port_addr_t;

extern devfs_ops_t serial_ops;

int32_t serial_init();                                                  // setup the serial communication
int32_t serial_register();                                              // register serial devices
int32_t serial_write(serial_port_addr_t addr, char *msg);               // write to the port
int32_t serial_read(serial_port_addr_t addr, char *msg, uint64_t size); // read from the port

#endif
