#pragma once

#ifndef __ASSEMBLY__

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

typedef signed char  int8_t;
typedef signed short int16_t;
typedef signed int   int32_t;
typedef signed long  int64_t;

typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap)          __builtin_va_end(ap)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_copy(dest, src)  __builtin_va_copy(dest, src)

typedef signed short mode_t;
typedef signed int   pid_t;
typedef unsigned char bool;

#endif

#define NULL (0)

#define UINT8_MAX  (0xff)
#define UINT16_MAX (0xffff)
#define UINT32_MAX (0xffffffff)
#define UINT64_MAX (0xffffffffffffffff)

#define UINT8_MIN  (0)
#define UINT16_MIN (0)
#define UINT32_MIN (0)
#define UINT64_MIN (0)

#define INT8_MAX  (0x7f)
#define INT16_MAX (0x7fff)
#define INT32_MAX (0x7fffffff)
#define INT64_MAX (0x7fffffffffffffff)

#define INT8_MIN  (-INT8_MAX)
#define INT16_MIN (-INT16_MAX)
#define INT32_MIN (-INT32_MAX)
#define INT64_MIN (-INT64_MAX)

#define true  (1)
#define false (0)
#define TRUE  (1)
#define FALSE (0)
