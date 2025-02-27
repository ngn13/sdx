#include "core/serial.h"
#include "video.h"

#include "util/math.h"
#include "util/mem.h"
#include "util/printk.h"
#include "util/string.h"

#include "config.h"

uint64_t __print(char *msg) {
  uint64_t i = 0;

  if (NULL == msg)
    return i;

  // write to video memory
  for (; msg[i] != 0; i++)
    video_write(msg[i]);

  // write to serial output
  serial_write(SERIAL_PORT_COM1, msg);

  return i;
}

uint64_t __print_char(char c) {
  char tmp[2] = {c, 0};
  return __print(tmp);
}

uint64_t __print_pad(uint64_t size) {
  for (uint64_t i = 0; i < size; i++)
    __print_char('0');
  return size;
}

uint64_t vprintf(char *fmt, va_list args) {
#define cmp(x) (*fmt == x)

  uint64_t size      = 0;
  bool     formatted = false;

  for (; *fmt != 0; fmt++) {
    if (!formatted && *fmt == '%') {
      formatted = true;
      continue;
    }

    else if (!formatted) {
      __print_char(*fmt);
      size++;
      continue;
    }

    // dword (integer)
    if (cmp('d')) {
      int32_t d = va_arg(args, int32_t);
      char    str[gd(d) + 1];

      itod(d, str);
      size += __print(str);
    }

    // character
    else if (cmp('c')) {
      uint8_t c      = va_arg(args, int32_t);
      char    str[2] = {c, 0};

      size += __print(str);
    }

    // long
    else if (cmp('l')) {
      int64_t l = va_arg(args, int64_t);
      char    str[gd(l) + 1];

      itod(l, str);
      size += __print(str);
    }

    // unsigned long
    else if (cmp('u')) {
      uint64_t l = va_arg(args, uint64_t);
      char     str[gd(l) + 1];

      itou(l, str);
      size += __print(str);
    }

    // hex
    else if (cmp('x')) {
      uint64_t x = va_arg(args, uint64_t);
      char     str[gdu(x) + 1];

      itoh(x, str);
      size += __print(str);
    }

    // pointer
    else if (cmp('p')) {
      uint64_t p = va_arg(args, uint64_t);
      char     str[gdu(p) + 1];
      uint64_t ps = 0;

      size += __print_pad(16 - itoh(p, str));
      size += __print(str);
    }

    // string
    else if (cmp('s')) {
      char *str = va_arg(args, char *);
      size += __print(str);
    }

    // GUID
    else if (cmp('g')) {
      uint8_t *guid = va_arg(args, uint8_t *), gi = 0, i = 0;
      uint64_t section_val = 0;
      char     str[14];

      // format: 00112233-0011-0011-1100-554433221100

      for (i = 0; i < 4; i++)
        section_val += pow(UINT8_MAX + 1, i) * guid[gi++];

      size += __print_pad(8 - itoh(section_val, str));
      size += __print(str);
      size += __print_char('-');
      section_val = 0;

      for (i = 0; i < 2; i++)
        section_val += pow(UINT8_MAX + 1, i) * guid[gi++];

      size += __print_pad(4 - itoh(section_val, str));
      size += __print(str);
      size += __print_char('-');
      section_val = 0;

      for (i = 0; i < 2; i++)
        section_val += pow(UINT8_MAX + 1, i) * guid[gi++];

      size += __print_pad(4 - itoh(section_val, str));
      size += __print(str);
      size += __print_char('-');
      section_val = 0;

      gi++;

      for (i = 0; i < 2; i++)
        section_val += pow(UINT8_MAX + 1, i) * guid[gi--];

      size += __print_pad(4 - itoh(section_val, str));
      size += __print(str);
      size += __print_char('-');
      section_val = 0;

      gi = 15;

      for (i = 0; i < 6; i++)
        section_val += pow(UINT8_MAX + 1, i) * guid[gi--];

      size += __print_pad(12 - itoh(section_val, str));
      size += __print(str);
    }

    formatted = false;
  }

  va_end(args);
  return size;
}

uint64_t printf(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  return vprintf(fmt, args);
}

uint64_t printk(enum printk_level level, char *fmt, ...) {
  uint64_t size = 0;
  va_list  args;

  va_start(args, fmt);

  switch (level) {

  case KERN_INFO:
    video_bg_set(VIDEO_COLOR_BLACK);
    video_fg_set(VIDEO_COLOR_LIGHT_BLUE);
    size += __print("INFO");
    break;

  case KERN_WARN:
    video_bg_set(VIDEO_COLOR_BLACK);
    video_fg_set(VIDEO_COLOR_YELLOW);
    size += __print("WARN");
    break;

  case KERN_FAIL:
    video_bg_set(VIDEO_COLOR_BLACK);
    video_fg_set(VIDEO_COLOR_LIGHT_RED);
    size += __print("FAIL");
    break;

  case KERN_DEBG:
    if (!CONFIG_DEBUG)
      return 0;
    video_bg_set(VIDEO_COLOR_BLACK);
    video_fg_set(VIDEO_COLOR_BROWN);
    size += __print("DEBG");
    break;
  }

  video_bg_set(VIDEO_COLOR_BLACK);
  video_fg_set(VIDEO_COLOR_WHITE);
  __print_char(' ');
  size += 1;

  size += vprintf(fmt, args);

  return size;
}

uint64_t dump(void *buffer, uint64_t size) {
  uint64_t res = 0, i = 0;

  for (; i < size; i++) {
    if (i % 10 == 0) {
      if (i != 0 && ++res)
        __print_char('\n');
      res += __print("      ");
    }

    if (printf("%x", ((uint8_t *)buffer)[i]) == 1)
      __print_char('0');
    __print_char(' ');

    res += 3;
  }

  if (res > 0) {
    __print_char('\n');
    res++;
  }

  return res;
}
