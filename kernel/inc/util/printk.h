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

#ifdef PPREFIX

#define pinfof(m, a...) printk(KERN_INFO, PPREFIX " " m "\n", a)
#define pinfo(m)        printk(KERN_INFO, PPREFIX " " m "\n")

#define pwarnf(m, a...) printk(KERN_WARN, PPREFIX " " m "\n", a)
#define pwarn(m)        printk(KERN_WARN, PPREFIX " " m "\n")

#define pfailf(m, a...) printk(KERN_FAIL, PPREFIX " " m "\n", a)
#define pfail(m)        printk(KERN_FAIL, PPREFIX " " m "\n")

#define pdebgf(m, a...) printk(KERN_DEBG, PPREFIX " " m "\n", a)
#define pdebg(m)        printk(KERN_DEBG, PPREFIX " " m "\n")

#else

#define pinfof(m, a...) printk(KERN_INFO, m "\n", a)
#define pinfo(m)        printk(KERN_INFO, m "\n")

#define pwarnf(m, a...) printk(KERN_WARN, m "\n", a)
#define pwarn(m)        printk(KERN_WARN, m "\n")

#define pfailf(m, a...) printk(KERN_FAIL, m "\n", a)
#define pfail(m)        printk(KERN_FAIL, m "\n")

#define pdebgf(m, a...) printk(KERN_DEBG, m "\n", a)
#define pdebg(m)        printk(KERN_DEBG, m "\n")

#endif
