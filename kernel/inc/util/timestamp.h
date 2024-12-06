#include "types.h"

typedef uint64_t timestamp_t; // yes we are using UNIX timestamps

timestamp_t timestamp_calc(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
