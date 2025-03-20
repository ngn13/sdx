#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

// spinlock

typedef uint8_t spinlock_t;

void spinlock_acquire(spinlock_t *lock);
#define spinlock_release(lock) (*(lock) = 0)
#define spinlock_locked(lock)  (*(lock) & 1)
#define spinlock_init(lock)    spinlock_release(lock)

#endif
