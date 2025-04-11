#include "core/ps2.h"
#include "types.h"
#include "errno.h"

// TODO: implement port 2 functions as well

void ps2_second_flush(ps2_port_t *port) {
  return;
}

int32_t ps2_second_read(ps2_port_t *port, uint8_t *data, bool timeout) {
  return -ENOSYS;
}

int32_t ps2_second_write(ps2_port_t *port, uint8_t data, bool timeout) {
  return -ENOSYS;
}

int32_t ps2_second_cmd(ps2_port_t *port, uint8_t cmd) {
  return -ENOSYS;
}

int32_t ps2_second_enable(ps2_port_t *port) {
  return -ENOSYS;
}

int32_t ps2_second_disable(ps2_port_t *port) {
  return -ENOSYS;
}

int32_t ps2_second_start(ps2_port_t *port) {
  return -ENOSYS;
}

int32_t ps2_second_stop(ps2_port_t *port) {
  return -ENOSYS;
}

// define the second PS2 port structure
ps2_port_t ps2_second_port = {
    .name = "second port",

    .enable  = ps2_second_enable,
    .disable = ps2_second_disable,

    .start = ps2_second_start,
    .stop  = ps2_second_stop,

    .flush = ps2_second_flush,
    .write = ps2_second_write,
    .read  = ps2_second_read,
    .cmd   = ps2_second_cmd,
};
