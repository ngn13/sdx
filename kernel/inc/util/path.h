#pragma once
#include "limits.h"
#include "types.h"

#define path_is_absolute(p) (*p == '/')
int32_t path_sanitize(char out[PATH_MAX + 1], char *in);
int32_t path_getcwd(char out[PATH_MAX + 1]);
