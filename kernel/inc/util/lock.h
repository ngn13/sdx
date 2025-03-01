#pragma once
#include "types.h"

// spinlock

typedef uint8_t spinlock_t;

void spinlock_acquire(spinlock_t *lock);
#define spinlock_release(lock) (*(lock) = 0)
#define spinlock_init(lock)    spinlock_release(lock)
