#pragma once
#include "types.h"

// spinlock

typedef uint64_t spinlock_t;

#define spinlock_init(lock)      lock = 0    // initialize the spinlock
#define spinlock_is_locked(lock) (lock != 0) // check if the spinlock is locked
#define spinlock_lock(lock)      ++lock
#define spinlock_unlock(lock)                                                                                          \
  do {                                                                                                                 \
    if (lock > 0)                                                                                                      \
      --lock;                                                                                                          \
  } while (0)
