#pragma once
#include "signal.h"
#include "types.h"

#define PAGE_SIZE (4096)
#define PATH_MAX  (PAGE_SIZE)
#define NAME_MAX  (255)
#define ARG_MAX   (PAGE_SIZE)
#define ENV_MAX   (INT32_MAX)
#define SIG_MAX   (SIGSEGV)
#define SIG_MIN   (SIGHUP)
#define PID_MAX   (INT32_MAX)
#define FD_MAX    (UINT8_MAX)
