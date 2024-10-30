#pragma once
#include "types.h"

bool bzero(void *data, int64_t size);
bool memcpy(void *dst, void *src, int64_t size);
void memswap(char *x, char *y);
