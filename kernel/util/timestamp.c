#include "util/timestamp.h"

#define IS_LEAP(y) ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))

uint8_t __days_in_month(uint8_t month, uint16_t year) {
  switch (month) {
  case 1:
  case 3:
  case 5:
  case 7:
  case 8:
  case 10:
  case 12:
    return 31;
  case 4:
  case 6:
  case 9:
  case 11:
    return 30;
  case 2:
    return IS_LEAP(year) ? 29 : 28;
  default:
    return 0; // Invalid month
  }
}

#define SECS_PER_MINUTE      (60)
#define SECS_PER_HOUR        (60 * SECS_PER_MINUTE)
#define SECS_PER_DAY         (24 * SECS_PER_HOUR)
#define SECS_PER_MONTH(m, y) (__days_in_month(m, y) * SECS_PER_DAY)
#define SECS_PER_YEAR        (365 * SECS_PER_DAY)
#define SECS_PER_LEAP        (366 * SECS_PER_DAY)

timestamp_t timestamp_calc(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  timestamp_t res = 0;
  uint16_t    i   = 0;

  // year
  for (i = 1970; i < year; i++)
    res += IS_LEAP(i) ? SECS_PER_LEAP : SECS_PER_YEAR;

  // month
  for (i = 1; i < month; i++)
    res += SECS_PER_MONTH(i, year);

  res += (day - 1) * SECS_PER_DAY; // day
  res += hour * SECS_PER_HOUR;     // hour
  res += minute * SECS_PER_MINUTE; // minute
  res += second;                   // second

  return res;
}
