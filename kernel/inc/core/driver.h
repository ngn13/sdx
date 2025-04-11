#pragma once

#ifndef __ASSEMBLY__
#include "types.h"

typedef struct driver {
  const char *name;
  bool        loaded;
  int32_t (*load)();
  int32_t (*unload)();
  struct driver *depends[];
} driver_t;

#define driver_extern(_name) extern driver_t _name##_driver;
#define driver_new(_name, _load, _unload, ...)                                                                         \
  driver_t _name##_driver = {                                                                                          \
      .name    = #_name,                                                                                               \
      .load    = _load,                                                                                                \
      .unload  = _unload,                                                                                              \
      .depends = {__VA_ARGS__ NULL},                                                                                   \
  }

void drivers_load();
void drivers_unload();

#endif
