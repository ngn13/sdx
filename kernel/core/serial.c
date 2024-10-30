#include "core/serial.h"
#include "util/io.h"
#include "util/mem.h"
#include "util/printk.h"

/*

 * functions for serial ports
 * see https://wiki.osdev.org/Serial_Ports

*/

struct serial_port {
  serial_port_addr_t addr;
  bool               available;
};

struct serial_port serial_ports[] = {
    {.addr = SERIAL_PORT_COM1, .available = false},
    {.addr = SERIAL_PORT_COM2, .available = false},
    {.addr = SERIAL_PORT_COM3, .available = false},
    {.addr = SERIAL_PORT_COM4, .available = false},
    {.addr = SERIAL_PORT_COM5, .available = false},
    {.addr = SERIAL_PORT_COM6, .available = false},
    {.addr = SERIAL_PORT_COM7, .available = false},
    {.addr = SERIAL_PORT_COM8, .available = false},
};

#define serial_port_count() (sizeof(serial_ports) / sizeof(struct serial_port))

enum serial_port_offsets {
  // offset 0 is used for read/write while DLAB is DISABLED
  SERIAL_OFF_READ  = 0,
  SERIAL_OFF_WRITE = 0,

  // ofset 0 is used for accessing LSB of baud rate while DLAB is ENABLED
  SERIAL_OFF_BAUD_DIV_LSB = 0,

  // offset 1 is used for accessing interrupt enable while DLAB is DISABLED
  SERIAL_OFF_INTERRUPT_ENABLE = 1,

  // offset 1 is used for accessing MSB of baud rate while DLAB is ENABLED
  SERIAL_OFF_BAUD_DIV_MSB = 1,

  // offset 2
  SERIAL_OFF_INTERRUPT_ID = 2,
  SERIAL_OFF_FIFO_CONTROL = 2,

  // offset 3
  SERIAL_OFF_LINE_CONTROL = 3,

  // offset 4
  SERIAL_OFF_MODEM_CONTROL = 4,

  // offset 5
  SERIAL_OFF_LINE_STATUS = 5,

  // offset 6
  SERIAL_OFF_MODEM_STATUS = 6,

  // offset 7
  SERIAL_OFF_SCRATCH = 7,
};

#define out8_port(o, v) out8(port->addr + o, v)
#define in8_port(o)     in8(port->addr + o)

// enum and load available ports
bool serial_init() {
#define __out8_fail_cont(o, v)                                                                                         \
  if (!out8_port(o, v))                                                                                                \
    continue;

  // stores the current port address
  struct serial_port *port            = NULL;
  uint8_t             available_count = 0;

  for (uint8_t i = 0; i < serial_port_count(); i++) {
    port = &serial_ports[i];

    __out8_fail_cont(SERIAL_OFF_INTERRUPT_ENABLE, 0);  // disable interrupts
    __out8_fail_cont(SERIAL_OFF_LINE_CONTROL, 1 << 7); // enable DLAB

    /*

     * baud rate divisor can only be accessed when DLAB is enabled
     * the baud rate controls the speed, so the divisor is used to change it

     * by default UART runs at 115200 ticks, divisor is essentialy used to divide this value
     * and decrease the baud rate, lower and higher bits of the divisor is separately

    */
    __out8_fail_cont(SERIAL_OFF_BAUD_DIV_LSB, 3);
    __out8_fail_cont(SERIAL_OFF_BAUD_DIV_MSB, 0);

    // stop bit is used to transmit a number of bits after every char of the data
    __out8_fail_cont(SERIAL_OFF_LINE_CONTROL, 1 << 2);

    /*

     * used to setup the FIFO (First In, First Out)
     * - bit 7-6: interrupt trigger level
     * - bit 2  : clear transmist FIFO
     * - bit 1  : clear receive FIFO
     * - bit 0  : enable FIFO

    */
    __out8_fail_cont(SERIAL_OFF_FIFO_CONTROL, 0b11000111);

    // set Data Terminal Ready (DTR) and Request to Send (RTS) and loopback (used for testing)
    __out8_fail_cont(SERIAL_OFF_MODEM_CONTROL, 0b10011);

    /*

     * since the loopback is enabled, we can test the port by writing a char
     * then reading it back

     * if we get the same char then it's all good, else the port is not available

    */
    __out8_fail_cont(SERIAL_OFF_WRITE, 0x42);

    if (in8_port(SERIAL_OFF_READ) != 0x42)
      continue; // test failed

    // disable loopback
    __out8_fail_cont(SERIAL_OFF_MODEM_CONTROL, 0b1111);

    port->available = true;
    available_count++;
  }

  printk(KERN_INFO, "Serial: enumerated %d ports\n", available_count);

  for (uint8_t i = 0; i < serial_port_count(); i++) {
    if (!serial_ports[i].available)
      continue;

    printk(KERN_INFO,
        --available_count == 0 ? "        `- Address: 0x%x Index: %d\n" : "        |- Address: 0x%x Index: %d\n",
        serial_ports[i].addr,
        i);
  }

  return true;
}

struct serial_port *__serial_find(serial_port_addr_t addr) {
  for (uint8_t i = 0; i < serial_port_count(); i++) {
    if (serial_ports[i].addr == addr)
      return &serial_ports[i];
  }

  return NULL;
}

bool __serial_port_writeable(struct serial_port *port) {
  // interrrupt status, transmitter holding register empty (THRE) bit
  return in8_port(SERIAL_OFF_LINE_STATUS) & 1 << 5;
}

bool __serial_port_readable(struct serial_port *port) {
  // interrrupt status, data ready (DR) bit
  return in8_port(SERIAL_OFF_LINE_STATUS) & 1;
}

bool serial_write(serial_port_addr_t addr, char *msg) {
  struct serial_port *port = __serial_find(addr);

  if (NULL == port || !port->available)
    return false;

  for (; *msg != 0; msg++) {
    while (!__serial_port_writeable(port))
      continue;

    if (!out8_port(SERIAL_OFF_WRITE, *msg))
      return false;
  }

  return true;
}

bool serial_read(serial_port_addr_t addr, char *msg, uint64_t size) {
  struct serial_port *port = __serial_find(addr);
  uint64_t            cur  = 0;

  if (NULL == port || !port->available)
    return false;

  for (; cur < size; cur++) {
    while (!__serial_port_readable(port))
      continue;
    msg[cur] = in8_port(SERIAL_OFF_READ);
  }

  return true;
}
