#pragma once

#ifndef __ASSEMBLY__
#include "core/driver.h"
#include "types.h"

// state bits (see kbd_key_t.state)
#define KBD_KEY_STATE_LEFT_SHIFT  (1 << 0)
#define KBD_KEY_STATE_RIGHT_SHIFT (1 << 1)
#define KBD_KEY_STATE_LEFT_ALT    (1 << 2)
#define KBD_KEY_STATE_RIGHT_ALT   (1 << 3)
#define KBD_KEY_STATE_LEFT_CTRL   (1 << 4)
#define KBD_KEY_STATE_RIGHT_CTRL  (1 << 5)
#define KBD_KEY_STATE_SCROLL      (1 << 6)
#define KBD_KEY_STATE_NUM         (1 << 7)

typedef struct {
  // uint32_t code; // keycode
  bool    pressed; // pressed/released
  uint8_t state;   // other key states (shift, caps etc)
} kbd_key_t;

driver_extern(kbd);

int32_t kbd_load();   // register the keyboard device
int32_t kbd_unload(); // unregister the keyboard device

bool    kbd_is_available(); // check if the keyboard is available
int32_t kbd_read(char *c);  // read from the keyboard

#endif
