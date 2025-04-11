#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

// spinlock

typedef uint8_t spinlock_t;

#define spinlock_init(lock)   (*(lock) = 0)
#define spinlock_locked(lock) (*(lock) & 1)

void spinlock_acquire(spinlock_t *lock);
void spinlock_release();

#endif
