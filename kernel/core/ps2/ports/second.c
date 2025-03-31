#include "core/ps2.h"
#include "types.h"
#include "errno.h"

// TODO: implement port 2 functions as well

void ps2_second_flush() {
  return;
}

int32_t ps2_second_read(uint8_t *data) {
  return -ENOSYS;
}

int32_t ps2_second_write(uint8_t data) {
  return -ENOSYS;
}

int32_t ps2_second_cmd(uint8_t cmd) {
  return -ENOSYS;
}

int32_t ps2_second_enable(uint8_t *id) {
  return -ENOSYS;
}

int32_t ps2_second_disable() {
  return -ENOSYS;
}

int32_t ps2_second_start() {
  return -ENOSYS;
}

int32_t ps2_second_stop() {
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
