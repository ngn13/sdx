#pragma once
#include "types.h"

enum printk_level {
  KERN_INFO = 0,
  KERN_WARN = 1,
  KERN_FAIL = 2,
  KERN_DEBG = 3,
};

uint64_t vprintf(char *fmt, va_list args);
uint64_t printf(char *fmt, ...);
uint64_t printk(enum printk_level level, char *msg, ...);
uint64_t dump(void *buffer, uint64_t size);

#ifdef PPREFIX

#define pinfo(m, ...) printk(KERN_INFO, PPREFIX " " m "\n", ##__VA_ARGS__)
#define pwarn(m, ...) printk(KERN_WARN, PPREFIX " " m "\n", ##__VA_ARGS__)
#define pfail(m, ...) printk(KERN_FAIL, PPREFIX " " m "\n", ##__VA_ARGS__)
#define pdebg(m, ...) printk(KERN_DEBG, PPREFIX " " m "\n", ##__VA_ARGS__)

#else

#define pinfo(m, ...) printk(KERN_INFO, m "\n", ##__VA_ARGS__)
#define pwarn(m, ...) printk(KERN_WARN, m "\n", ##__VA_ARGS__)
#define pfail(m, ...) printk(KERN_FAIL, m "\n", ##__VA_ARGS__)
#define pdebg(m, ...) printk(KERN_DEBG, m "\n", ##__VA_ARGS__)

#endif
