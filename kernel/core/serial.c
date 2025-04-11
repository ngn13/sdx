#include "core/serial.h"
#include "core/im.h"
#include "core/pic.h"
#include "core/timer.h"

#include "sched/sched.h"
#include "sched/task.h"

#include "fs/fs.h"
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

#define SERIAL_DEV_NAME  "com"
#define SERIAL_DEV_MAJOR (3)

driver_new(serial, serial_load, serial_unload);

/*

 * functions for serial port communication
 * see https://wiki.osdev.org/Serial_Ports

*/

enum {
  /*

   * offset 0 is used for R/W while DLAB is DISABLED however when DLAB is
   * ENABLED, then it's used for accessing the LSB of the baud rate

  */
  SERIAL_OFF_READ         = 0,
  SERIAL_OFF_WRITE        = 0,
  SERIAL_OFF_BAUD_DIV_LSB = 0,

  /*

   * offset 1 is used for accessing the interrupt enable while DLAB is
   * DISABLED, when DLAB is ENABLED, it's used for accessing the MSB of
   * the baud rate

  */
  SERIAL_OFF_INTERRUPT_ENABLE = 1,
  SERIAL_OFF_BAUD_DIV_MSB     = 1,

  // other offsets
  SERIAL_OFF_INTERRUPT_ID  = 2,
  SERIAL_OFF_FIFO_CONTROL  = 2,
  SERIAL_OFF_LINE_CONTROL  = 3,
  SERIAL_OFF_MODEM_CONTROL = 4,
  SERIAL_OFF_LINE_STATUS   = 5,
  SERIAL_OFF_MODEM_STATUS  = 6,
  SERIAL_OFF_SCRATCH       = 7,
};

// line status register bits
#define SERIAL_LINE_STS_DR   (1 << 0) // data ready, set if data can be read
#define SERIAL_LINE_STS_OE   (1 << 1) // overrun error, set if there has been data lost
#define SERIAL_LINE_STS_PE   (1 << 2) // parity error, set if there was an error in the transmission
#define SERIAL_LINE_STS_FE   (1 << 3) // framing error, set if a stop bit was missing
#define SERIAL_LINE_STS_BI   (1 << 4) // break indicator, set if there is a break in data input
#define SERIAL_LINE_STS_THRE (1 << 5) // transmitter holding register empty
#define SERIAL_LINE_STS_TEMT (1 << 6) // transmitter empty

// line control register bits
#define SERIAL_LINE_CTRL_DATA_8 (0b11)   // 8 bits per char
#define SERIAL_LINE_CTRL_STOP   (1 << 2) // stop bit
#define SERIAL_LINE_CTRL_BREAK  (1 << 6) // break enable bit
#define SERIAL_LINE_CTRL_DLAB   (1 << 7) // divisor latch access bit

// interrupt enable register bits
#define SERIAL_INT_ENABLE_DR   (1 << 0) // send interrupt when received data is ready (see SERIAL_LINE_STS_DR)
#define SERIAL_INT_ENABLE_THRE (1 << 1) // send interrupt when THRE (see SERIAL_LINE_STS_THRE)

// first in first out (FIFO) control register bits
#define SERIAL_FIFO_ENABLE      (1 << 0)
#define SERIAL_FIFO_CLEAR_RECV  (1 << 1)
#define SERIAL_FIFO_CLEAR_TRANS (1 << 2)

// modem control register bits
#define SERIAL_MODEM_CTRL_DTR  (1 << 0) // data terminal ready
#define SERIAL_MODEM_CTRL_RTS  (1 << 1) // request to send
#define SERIAL_MODEM_CTRL_OUT1 (1 << 2) // OUT1 pin
#define SERIAL_MODEM_CTRL_OUT2 (1 << 3) // OUT2 pin (enables the IRQ)
#define SERIAL_MODEM_CTRL_LOOP (1 << 4) // loopback

// interrupt identification register bits
#define SERIAL_INT_ID_PENDING    (1 << 0)              // there's a pending interrupt
#define serial_int_id_state(iir) (((iir) >> 1) & 0b11) // interrupt state

// interrupt state
enum {
  SERIAL_INT_STATE_THRE = 1,
  SERIAL_INT_STATE_DR,
};

// defines and stores information about a serial communication port
struct serial_port {
  const serial_addr_t addr;       // address of the port
  uint8_t             minor;      // minor of port's device
  int32_t             interrupt;  // interrupt the port uses
  bool                available;  // is the port available
  spinlock_t          read_lock;  // read lock for the port
  spinlock_t          write_lock; // write lock for the port
};

// serial communication ports
struct serial_port serial_ports[] = {
    {SERIAL_PORT_COM1, false, pic_to_int(PIC_IRQ_COM1)},
    {SERIAL_PORT_COM2, false, pic_to_int(PIC_IRQ_COM2)},
    {SERIAL_PORT_COM3, false, pic_to_int(PIC_IRQ_COM1)}, // IRQ is shared with port 1
    {SERIAL_PORT_COM4, false, pic_to_int(PIC_IRQ_COM2)}, // IRQ is shared with port 2
    {SERIAL_PORT_COM5, false, -1                      },
    {SERIAL_PORT_COM6, false, -1                      },
    {SERIAL_PORT_COM7, false, -1                      },
    {SERIAL_PORT_COM8, false, -1                      },
    {SERIAL_PORT_COM8, false, -1                      },
    {NULL,             false, -1                      },
};

// used to loop through the ports
#define serial_port_foreach() for (struct serial_port *port = &serial_ports[0]; NULL != port->addr; port++)

// port I/O macros
#define serial_out(offset, value) out8(port->addr + offset, value)
#define serial_in(offset)         in8(port->addr + offset)

// offset/register macros (close your eyes)
#define serial_read8()   serial_in(SERIAL_OFF_READ)
#define serial_write8(c) serial_out(SERIAL_OFF_WRITE, c)

#define serial_modem_ctrl_read() serial_in(SERIAL_OFF_MODEM_CONTROL)
#define serial_fifo_read()       serial_in(SERIAL_OFF_FIFO_CONTROL)
#define serial_int_id_read()     serial_in(SERIAL_OFF_INTERRUPT_ID)
#define serial_int_enable_read() serial_in(SERIAL_OFF_INTERRUPT_ENABLE)
#define serial_line_sts_read()   serial_in(SERIAL_OFF_LINE_STATUS)
#define serial_line_ctrl_read()  serial_in(SERIAL_OFF_LINE_CONTROL)

#define serial_modem_ctrl_write(value) serial_out(SERIAL_OFF_MODEM_CONTROL, (value))
#define serial_fifo_write(value)       serial_out(SERIAL_OFF_FIFO_CONTROL, (value))
#define serial_int_id_write(value)     serial_out(SERIAL_OFF_INTERRUPT_ID, (value))
#define serial_int_enable_write(value) serial_out(SERIAL_OFF_INTERRUPT_ENABLE, (value))
#define serial_line_sts_write(value)   serial_out(SERIAL_OFF_LINE_STATUS, (value))
#define serial_line_ctrl_write(value)  serial_out(SERIAL_OFF_LINE_CONTROL, (value))

#define serial_modem_ctrl_set(mask) serial_modem_ctrl_write(serial_modem_ctrl_read() | (mask))
#define serial_int_enable_set(mask) serial_int_enable_write(serial_int_enable_read() | (mask))
#define serial_line_sts_set(mask)   serial_line_sts_write(serial_line_sts_read() | (mask))
#define serial_line_ctrl_set(mask)  serial_line_ctrl_write(serial_line_ctrl_read() | (mask))

#define serial_modem_ctrl_clear(mask) serial_modem_ctrl_write(serial_modem_ctrl_read() & ~(mask))
#define serial_int_enable_clear(mask) serial_int_enable_write(serial_int_enable_read() & ~(mask))
#define serial_line_sts_clear(mask)   serial_line_sts_write(serial_line_sts_read() & ~(mask))
#define serial_line_ctrl_clear(mask)  serial_line_ctrl_write(serial_line_ctrl_read() & ~(mask))

// other macros
#define serial_writeable() (serial_line_sts_read() & SERIAL_LINE_STS_THRE)
#define serial_readable()  (serial_line_sts_read() & SERIAL_LINE_STS_DR)

struct serial_port *__serial_port_by_addr(serial_addr_t addr) {
  // look for the port with the given address
  serial_port_foreach() {
    if (port->addr == addr)
      return port;
  }

  return NULL;
}

struct serial_port *__serial_port_by_inode(fs_inode_t *inode) {
  // look for the port with the given address
  serial_port_foreach() {
    if (port->minor == devfs_minor(inode->addr))
      return port;
  }

  return NULL;
}

int32_t __serial_port_write(struct serial_port *port, char c) {
  if (NULL == port || !port->available)
    return -EINVAL;

  if (port->interrupt == -1)
    // sleep until port is writeable
    while (!serial_writeable())
      timer_sleep(100);

  else
    // wait until port is writeable
    sched_block_until(TASK_BLOCK_OUTPUT, !serial_writeable());

  // write a single byte
  serial_write8(c);
  return 0;
}

int32_t __serial_port_read(struct serial_port *port, char *c) {
  if (NULL == c || NULL == port || !port->available)
    return -EINVAL;

  if (port->interrupt == -1)
    // sleep until port is readable
    while (!serial_readable())
      timer_sleep(100);

  else
    // wait until port is readable
    sched_block_until(TASK_BLOCK_INPUT, !serial_readable());

  // read a single byte
  *c = (char)serial_read8();
  return 0;
}

int64_t __serial_ops_read(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  struct serial_port *port     = __serial_port_by_inode(inode);
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
  spinlock_release();
  return cur_size;
}

int64_t __serial_ops_write(fs_inode_t *inode, uint64_t offset, uint64_t size, void *buffer) {
  struct serial_port *port     = __serial_port_by_inode(inode);
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
  spinlock_release();
  return cur_size;
}

// device operations for the serial port devices
devfs_ops_t serial_ops = {
    .open  = fs_default,
    .close = fs_default,
    .read  = __serial_ops_read,
    .write = __serial_ops_write,
};

void __serial_int_handler(im_stack_t *stack) {
  uint8_t iir = 0;

  serial_port_foreach() {
    // port does not support interrupts
    if (!port->available || port->interrupt == -1)
      continue;

    // check if there's any pending interrupt
    if (((iir = serial_int_id_read()) & SERIAL_INT_ID_PENDING))
      continue;

    switch (serial_int_id_state(iir)) {
    // ready for write
    case SERIAL_INT_STATE_THRE:
      sched_unblock(NULL, TASK_BLOCK_OUTPUT);
      break;

    case SERIAL_INT_STATE_DR:
      // read for read
      sched_unblock(NULL, TASK_BLOCK_INPUT);
      break;
    }
  }
}

int32_t serial_init() {
  uint8_t count = 0, indx = 0;

  // enum and load available ports
  serial_port_foreach() {
    // disable (all) interrupts
    serial_int_enable_write(0);

    /*

     * baud rate divisor can only be accessed when DLAB is enabled the baud rate controls the
     * speed, so the divisor is used to change it

     * by default UART runs at 115200 ticks, divisor is essentialy used to divide this value
     * and decrease the baud rate, lower and higher bits of the divisor is separately

    */
    serial_line_ctrl_set(SERIAL_LINE_CTRL_DLAB);
    serial_out(SERIAL_OFF_BAUD_DIV_LSB, 3);
    serial_out(SERIAL_OFF_BAUD_DIV_MSB, 0);

    /*

     * set the stop bit to 1, which means the controller will send a number of bits after
     * each char of data, which can be used to verify that communication is working properly
     * also set SERIAL_LINE_CTRL_DATA_8, which means we are going to use 8 bits per char

    */
    serial_line_ctrl_write(SERIAL_LINE_CTRL_STOP | SERIAL_LINE_CTRL_DATA_8);

    // enable and clear FIFO, set interrupt trigger level to 1 (single byte)
    serial_fifo_write(SERIAL_FIFO_ENABLE | SERIAL_FIFO_CLEAR_RECV | SERIAL_FIFO_CLEAR_TRANS);

    // setup the modem control register with loopback (used for testing)
    serial_modem_ctrl_set(SERIAL_MODEM_CTRL_DTR | SERIAL_MODEM_CTRL_RTS | SERIAL_MODEM_CTRL_LOOP);

    /*

     * since the loopback is enabled, we can test the port by writing a char
     * then reading it back, if we get the same char then it's all good,
     * otherwise the port is not available

    */
    serial_write8(0x42);

    if (serial_read8() != 0x42)
      continue; // test failed

    // disable loopback and enable IRQ
    serial_modem_ctrl_clear(SERIAL_MODEM_CTRL_LOOP);
    serial_modem_ctrl_set(SERIAL_MODEM_CTRL_OUT2);

    // enable the interrupts
    serial_int_enable_set(SERIAL_INT_ENABLE_DR | SERIAL_INT_ENABLE_THRE);
    // serial_int_enable_set(SERIAL_INT_ENABLE_DR);

    // setup port structure
    port->available = true;
    spinlock_init(&port->read_lock);
    spinlock_init(&port->write_lock);

    // increase the port counter
    count++;
  }

  // print enumerated port count
  serial_info("enumerated %d ports", count);

  // list enumerated ports
  serial_port_foreach() {
    // skip non available ports
    if (!port->available)
      continue;

    if (count <= 1)
      pinfo("        `- Address: 0x%x Index: %d", port->addr, indx);
    else
      pinfo("        |- Address: 0x%x Index: %d", port->addr, indx);

    indx++;
    count--;
  }

  return 0;
}

int32_t serial_load() {
  int32_t err   = 0;
  uint8_t minor = 0;

  // register the serial port device
  if ((err = devfs_register(SERIAL_DEV_MAJOR, SERIAL_DEV_NAME, &serial_ops)) != 0) {
    serial_fail("failed to register the serial device: %s", strerror(err));
    return err;
  }

  serial_port_foreach() {
    // skip if port is not available
    if (!port->available)
      continue;

    // add the handler for the port and unmask the IRQ
    if (port->interrupt != -1) {
      im_add_handler(port->interrupt, __serial_int_handler);
      pic_unmask(port->interrupt);
    }

    // create the serial port device
    if ((err = devfs_create(devfs_addr(SERIAL_DEV_MAJOR, minor), NULL, MODE_USRR | MODE_USRW)) != 0) {
      serial_fail("failed to create the serial device: %s", strerror(err));
      return err;
    }

    // save the device minor
    port->minor = minor;

    serial_info("registered the serial port device");
    pinfo("        |- Address: 0x%x", port->addr);
    pinfo("        `- Minor: %u", port->minor);
  }

  return 0;
}

int32_t serial_unload() {
  // unregister all the serial devices
  devfs_unregister(SERIAL_DEV_MAJOR);
  return 0;
}

int32_t serial_write(serial_addr_t addr, char *msg) {
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

int32_t serial_read(serial_addr_t addr, char *msg, uint64_t size) {
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
