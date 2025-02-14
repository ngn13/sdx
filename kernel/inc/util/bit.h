#pragma once

#define bit_set(t, b, v) (t) = (((t) & ~(1 << (b))) | (((v) << (b)) & (1 << (b))))
#define bit_get(t, b)    (((t) >> (b)) & 1)
