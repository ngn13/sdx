#pragma once
#include "types.h"

#define panic(m) _panic(__func__, m)      // little macro just to make it easier
void _panic(const char *func, char *msg); // defined in util/panic.S
