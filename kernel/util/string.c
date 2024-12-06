#include "util/string.h"
#include "util/math.h"

#include "video.h"

uint8_t strcmp(char *s1, char *s2) {
  if (NULL == s1 || NULL == s2)
    return -1;

  for (; *s1 != 0 && *s2 != 0; s1++, s2++) {
    if (*s1 != *s2)
      break;
  }

  return s1[0] - s2[0];
}

uint64_t strncmp(char *s1, char *s2, uint64_t len) {
  for (; len > 0; len--) {
    if (*s1 == 0)
      return 0;

    if (*s1 != *s2)
      return *s1 - *s2;

    s1++;
    s2++;
  }

  return 0;
}

char *strlwr(char *str) {
  for (; *str != 0; str++)
    *str |= 32;
  return str;
}

uint64_t strlen(char *str) {
  if (NULL == str)
    return 0;

  uint64_t len = 0;
  while (str[len] != 0)
    len++;
  return len;
}

bool strrev(char *str) {
  if (NULL == str)
    return false;

  uint64_t indx  = strlen(str);
  char    *strcp = str;
  char     c     = 0;

  if (indx <= 1)
    return true;
  indx--;

  while (str != strcp + indx) {
    c           = *str;
    *str        = strcp[indx];
    strcp[indx] = c;

    str++;

    if (str == strcp + indx)
      break;

    indx--;
  }

  return true;
}

uint64_t itou(uint64_t val, char *dst) {
  if (NULL == dst)
    return NULL;

  uint64_t num = abs(val), i = 0;

  do {
    dst[i++] = (num % 10) + '0';
    num /= 10;
  } while (num != 0);

  dst[i++] = 0;

  strrev(dst);
  return i - 1;
}

uint64_t itod(int64_t val, char *dst) {
  if (NULL == dst)
    return NULL;

  uint64_t num = abs(val), i = 0;

  do {
    dst[i++] = (num % 10) + '0';
    num /= 10;
  } while (num != 0);

  if (val < 0)
    dst[i++] = '-';
  dst[i++] = 0;

  strrev(dst);
  return i - 1;
}

uint64_t itoh(uint64_t val, char *dst) {
  if (NULL == dst)
    return NULL;

  char     hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  uint64_t num = abs(val), i = 0;

  do {
    dst[i++] = hex_chars[(num % 16)];
    num /= 16;
  } while (num != 0);

  dst[i++] = 0;

  strrev(dst);
  return i - 1;
}

char *strchr(char *s, char c) {
  if (NULL == s)
    return NULL;

  for (; *s != 0; s++)
    if (*s == c)
      return s;

  return NULL;
}

char *strstr(char *s1, char *s2) {
  char    *p   = s1;
  uint64_t len = strlen(s2);

  if (len == 0)
    return s1;

  for (; (p = strchr(p, *s2)) != 0; p++)
    if (strncmp(p, s2, len) == 0)
      return p;

  return NULL;
}
