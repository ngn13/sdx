#pragma once
#include "types.h"

uint8_t  strcmp(char *s1, char *s2);
uint64_t strncmp(char *s1, char *s2, uint64_t len);
bool     strrev(char *str);
uint64_t strlen(const char *str);
char    *strstr(char *s1, char *s2);
char    *strchr(char *s, char c);
char    *strncpy(char *dst, const char *src, uint64_t dsize);
#define streq(s1, s2) (strcmp(s1, s2) == 0)
const char *strerror(int32_t err);

uint64_t itou(uint64_t val, char *dst);
uint64_t itod(int64_t val, char *dst);
uint64_t itoh(uint64_t val, char *dst);
