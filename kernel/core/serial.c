#include "core/serial.h"
#include "sched/sched.h"
#include "fs/devfs.h"

#include "util/printk.h"
#include "util/string.h"
#include "util/lock.h"
#include "util/io.h"

#include "types.h"
#include "errno.h"

#define serial_debg(f, ...) pdebg("Serial: " f, ##__VA_ARGS__)
#define serial_info(f, ...) pinfo("Serial: " f, ##__VA_ARGS__)
#define serial_fail(f, ...) pfail("Serial: " f, ##__VA_ARGS__)

/*

 * functions for serial port communication
 * see https://wiki.osdev.org/Serial_Ports

*/

struct serial_port {
  const char        *name;       // name of the port
  serial_port_addr_t addr;       // address of the port
  int32_t            dev;        // device address for the port
  bool               available;  // is the port available
  spinlock_t         read_lock;  // read lock for the port
  spinlock_t         write_lock; // write lock for the port
};

enum {
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

// serial communication ports
struct serial_port serial_ports[] = {
    {"com1", SERIAL_PORT_COM1, false},
    {"com2", SERIAL_PORT_COM2, false},
    {"com3", SERIAL_PORT_COM3, false},
    {"com4", SERIAL_PORT_COM4, false},
    {"com5", SERIAL_PORT_COM5, false},
    {"com6", SERIAL_PORT_COM6, false},
    {"com7", SERIAL_PORT_COM7, false},
    {"com8", SERIAL_PORT_COM8, false},
    {"com9", SERIAL_PORT_COM8, false},
    {NULL,   NULL,             false},
};

#define __out8_port(o, v) out8(port->addr + o, v)
#define __in8_port(o)     in8(port->addr + o)
#define __out8_fail_cont(o, v)                                                                                         \
  if (!__out8_port(o, v))                                                                                              \
    continue;

struct serial_port *__serial_port_by_addr(serial_port_addr_t addr) {
  struct serial_port *port = &serial_ports[0];

  // look for the port with the given address
  for (; NULL != port->addr; port++) {
    if (port->addr == addr)
      return port;
  }

  return NULL;
}

struct serial_port *__serial_port_by_dev(int32_t dev) {
  struct serial_port *port = &serial_ports[0];

  // look for the port with the given address
  for (; NULL != port->addr; port++) {
    if (port->dev == dev)
      return port;
  }

  return NULL;
}

// interrrupt status, transmitter holding register empty (THRE) bit
#define __serial_port_writeable() (__in8_port(SERIAL_OFF_LINE_STATUS) & 1 << 5)

// interrrupt status, data ready (DR) bit
#define __serial_port_readable() (__in8_port(SERIAL_OFF_LINE_STATUS) & 1)

int32_t __serial_port_write(struct serial_port *port, char c) {
  if (NULL == port || !port->available)
    return -EINVAL;

  // wait until port is waitable
  while (!__serial_port_writeable())
    sched_wait();

  // write a single byte
  if (!__out8_port(SERIAL_OFF_WRITE, c))
    return -EIO;

  return 0;
}

int32_t __serial_port_read(struct serial_port *port, char *c) {
  if (NULL == c || NULL == port || !port->available)
    return -EINVAL;

  // wait until port is readble
  while (!__serial_port_readable())
    sched_wait();

  // read a single byte
  *c = (char)__in8_port(SERIAL_OFF_READ);
  return 0;
}

int32_t __serial_open(fs_inode_t *inode) {
  // try to obtain the port from the inode
  if (NULL == __serial_port_by_dev(inode->addr))
    return -EFAULT;

  // success
  return 0;
}

int32_t __serial_close(fs_inode_t *inode) {
  // try to obtain the port from the inode
  if (NULL == __serial_port_by_dev(inode->addr))
    return -EFAULT;

  // success
  return 0;
}

int64_t __serial_read(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  struct serial_port *port     = __serial_port_by_dev(inode->addr);
  uint64_t            cur_size = 0;
  int64_t             err      = 0;

  // lock the port until we are done reading
  spinlock_acquire(&port->read_lock);

  // read each char into the buffer
  for (; cur_size < size; cur_size++, buffer++) {
    if ((err = __serial_port_read(port, (char *)buffer)) != 0)
      return err;
  }

  // release the lock
  spinlock_release(&port->read_lock);
  return cur_size;
}

int64_t __serial_write(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  struct serial_port *port     = __serial_port_by_dev(inode->addr);
  uint64_t            cur_size = 0;
  int64_t             err      = 0;

  // lock the port until we are done writing
  spinlock_acquire(&port->write_lock);

  // write the each char to the port
  for (; cur_size < size; cur_size++, buffer++) {
    if ((err = __serial_port_write(port, *(char *)buffer)) != 0)
      return err;
  }

  // release the lock
  spinlock_release(&port->write_lock);
  return cur_size;
}

// device operations for the serial port devices
devfs_ops_t serial_ops = {
    .open  = __serial_open,
    .close = __serial_close,
    .read  = __serial_read,
    .write = __serial_write,
};

int32_t serial_init() {
  // stores the current port
  struct serial_port *port  = NULL;
  uint8_t             count = 0, indx = 0;

  // enum and load available ports
  for (port = &serial_ports[0]; NULL != port->addr; port++) {
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
     * then reading it back, if we get the same char then it's all good,
     * otherwise the port is not available

    */
    __out8_fail_cont(SERIAL_OFF_WRITE, 0x42);

    if (__in8_port(SERIAL_OFF_READ) != 0x42)
      continue; // test failed

    // disable loopback
    __out8_fail_cont(SERIAL_OFF_MODEM_CONTROL, 0b1111);

    // setup port data
    port->dev       = -1;
    port->available = true;
    spinlock_init(&port->read_lock);
    spinlock_init(&port->write_lock);

    // increase the port counter
    count++;
  }

  // print enumerated port count
  serial_info("enumerated %d ports", count);

  // list enumerated ports
  for (port = &serial_ports[0], indx = 0; NULL != port->addr; count--, indx++, port++) {
    // skip non available ports
    if (!port->available)
      continue;

    if (count <= 1)
      pinfo("        `- Address: 0x%x Index: %d", port->addr, indx);
    else
      pinfo("        |- Address: 0x%x Index: %d", port->addr, indx);
  }

  return 0;
}

int32_t serial_register() {
  struct serial_port *port     = &serial_ports[0];
  int32_t             dev_addr = 0;

  for (; NULL != port->addr; port++) {
    // skip if port is not available
    if (!port->available)
      continue;

    // register the serial port device
    if ((dev_addr = devfs_device_register(port->name, &serial_ops, MODE_USRR | MODE_USRW)) < 0) {
      serial_fail("failed to register the %s device: %s", port->name, strerror(dev_addr));
      return dev_addr;
    }

    // save the device address
    port->dev = dev_addr;

    serial_info("registered the serial port device");
    pinfo("        |- Name: %s (0x%x)", port->name, port->addr);
    pinfo("        `- Device: %u", port->dev);
  }

  return 0;
}

int32_t serial_write(serial_port_addr_t addr, char *msg) {
  struct serial_port *port = __serial_port_by_addr(addr);
  int32_t             err  = 0;

  // check the port
  if (NULL == port)
    return -EINVAL;

  // if locked, can't be used by the kernel
  if (spinlock_locked(&port->write_lock))
    return -EFAULT;

  for (; *msg != 0; msg++) {
    // write each char of the message
    if ((err = __serial_port_write(port, *msg)) != 0)
      return err;
  }

  return 0;
}

int32_t serial_read(serial_port_addr_t addr, char *msg, uint64_t size) {
  struct serial_port *port = __serial_port_by_addr(addr);
  uint64_t            cur  = 0;
  int32_t             err  = 0;

  // check the port
  if (NULL == port)
    return -EINVAL;

  // if locked, can't be used by the kernel
  if (spinlock_locked(&port->read_lock))
    return -EFAULT;

  for (; cur < size; cur++) {
    // read each char from the serial port
    if ((err = __serial_port_read(port, &msg[cur])) != 0)
      return err;
  }

  return 0;
}
