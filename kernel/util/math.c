#include "util/math.h"

uint64_t abs(int64_t val) {
  return val >= 0 ? val : -val;
}

uint64_t gd(int64_t val) {
  if (val < 10 && val >= 0)
    return 1;

  if (val > -10 && val < 0)
    return 2;

  return 1 + gd(val / 10);
}

uint64_t gdu(uint64_t val) {
  if (val < 10)
    return 1;
  return 1 + gdu(val / 10);
}

uint64_t div_ceil(uint64_t x, uint64_t y) {
  return (1 + ((x - 1) / y));
}

uint64_t div_floor(uint64_t x, uint64_t y) {
  return x / y;
}

uint64_t pow(uint64_t n, uint64_t e) {
  if (e == 0)
    return 1;

  uint64_t r = n;

  for (; e > 1; e--)
    r *= n;

  return r;
}

uint64_t round_up(uint64_t x, uint64_t f) {
  if (!f)
    return x;
  return (x + f - 1) / f * f;
}

uint64_t round_down(uint64_t x, uint64_t f) {
  if (!f)
    return x;
  return x / f * f;
}
