#pragma once
#ifndef __ASSEMBLY__

#include "util/lock.h"
#include "fs/devfs.h"

#include "limits.h"
#include "types.h"

// describes a TTY device
typedef struct tty {
  char    name[NAME_MAX + 1]; // name of the TTY device ("tty1", "tty2" etc.)
  int32_t dev;                // devfs device address of the TTY device
  struct tty_ops {
    int32_t (*open)(struct tty *tty);
    int32_t (*close)(struct tty *tty);
    int64_t (*read)(struct tty *tty, uint64_t offset, uint64_t size, void *buf);
    int64_t (*write)(struct tty *tty, uint64_t offset, uint64_t size, void *buf);
  }          *ops; // TTY device operations
  spinlock_t  lock;
  struct tty *next; // next device in the TTY device list
} tty_t;

// decribes the operations on a TYY device
typedef struct tty_ops tty_ops_t;

// core/tty/tty.c
tty_t  *tty_register(tty_ops_t *ops, char *name); // register a TTY device
int32_t tty_unregister(char *name);               // remove a registered TTY deivce

#endif
