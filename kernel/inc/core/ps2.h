#pragma once

#ifndef __ASSEMBLY__
#include "types.h"

// initialize the PS/2 controller
int32_t ps2_init();

// read and write data
uint8_t ps2_read();
void    ps2_write(uint8_t data);

// send commands and receive responses
uint8_t ps2_cmd(uint8_t cmd);
void    ps2_cmd_with(uint8_t cmd, uint8_t data);

#endif
