#pragma once
#include <types.h>

typedef enum serial_port_addr {
  SERIAL_PORT_COM1 = 0x3F8,
  SERIAL_PORT_COM2 = 0x2F8,
  SERIAL_PORT_COM3 = 0x3E8,
  SERIAL_PORT_COM4 = 0x2E8,
  SERIAL_PORT_COM5 = 0x5F8,
  SERIAL_PORT_COM6 = 0x4F8,
  SERIAL_PORT_COM7 = 0x5E8,
  SERIAL_PORT_COM8 = 0x4E8,
} serial_port_addr_t;

int32_t serial_init();
int32_t serial_write(serial_port_addr_t addr, char *msg);
int32_t serial_read(serial_port_addr_t addr, char *msg, uint64_t size);
