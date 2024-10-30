#include "boot/multiboot.h"

#include "util/io.h"
#include "util/mem.h"

#include "video.h"

struct fb_data {
  uint32_t x, y;
  uint8_t  color;
} fb_data;

typedef uint8_t fb_data_color[2];

uint8_t fb_color_map[] = {
    0x0, // VIDEO_BLACK
    0xf, // VIDEO_WHITE
    0x1, // VIDEO_BLUE
    0x2, // VIDEO_GREEN
    0x4, // VIDEO_RED
    0x6, // VIDEO_BROWN
    0xe, // VIDEO_YELLOW
    0x9, // VIDEO_LIGHT_BLUE
    0xa, // VIDEO_LIGHT_GREEN
    0xc, // VIDEO_LIGHT_RED
};

#define FB_WIDTH     mb_fb_width
#define FB_HEIGHT    mb_fb_height
#define FB_CHAR_SIZE mb_fb_size
#define FB_ADDR      ((uint64_t)mb_fb_addr)

#define __fb_pos()         ((fb_data.y * (FB_WIDTH * FB_CHAR_SIZE)) + (fb_data.x * FB_CHAR_SIZE))
#define __fb_cursor_pos()  ((fb_data.y * FB_WIDTH) + fb_data.x)
#define __fb_color_count() (sizeof(fb_color_map) / sizeof(uint8_t))
#define __fb_color(i)      (i > __fb_color_count()) || i < 0 ? 0 : fb_color_map[i]
#define __fail_return(x)                                                                                               \
  if (!x)                                                                                                              \
  return false

bool fb_init() {
  bzero(&fb_data, sizeof(struct fb_data));
  return true;
}

void fb_clear() {
  uint64_t max = FB_WIDTH * FB_HEIGHT;

  for (uint32_t i = 0; i < max; i++)
    ((short *)FB_ADDR)[i] = 0;

  // move the cursor to top
  fb_data.x = 0;
  fb_data.y = 0;
}

void __fb_scroll() {
  if (fb_data.y < FB_HEIGHT)
    return;

  uint32_t line = 0, i = 0;

  for (; line < FB_HEIGHT; line++) {
    if (line == FB_HEIGHT - 1) {
      /*for (i = line * FB_WIDTH; i < (line + 1) * FB_WIDTH; i++)
        ((short *)FB_ADDR)[i] = 0;
      continue;*/
    }

    for (i = line * FB_WIDTH; i < (line + 1) * FB_WIDTH; i++)
      ((short *)FB_ADDR)[i] = ((short *)FB_ADDR)[i + FB_WIDTH];
  }

  fb_data.y--;
  __fb_scroll(); // keep going till we make sure the height is good
}

bool __fb_cursor_update() {
  __fb_scroll();
  uint32_t pos = __fb_cursor_pos();

  __fail_return(out8(0x3D4, 0x0F));
  __fail_return(out8(0x3D5, (uint8_t)(pos & 0xFF)));

  __fail_return(out8(0x3D4, 0x0E));
  __fail_return(out8(0x3D5, (uint8_t)((pos >> 8) & 0xFF)));

  return true;
}

bool fb_cursor_hide() {
  __fail_return(out8(0x3d4, 0x0a));
  __fail_return(out8(0x3d5, 0x20));

  return true;
}

bool fb_cursor_show() {
  __fail_return(out8(0x3D4, 0x0A));
  __fail_return(out8(0x3D5, (in8(0x3D5) & 0xC0) | 0));

  __fail_return(out8(0x3D4, 0x0B));
  __fail_return(out8(0x3D5, (in8(0x3D5) & 0xE0) | 1));

  return true;
}

bool fb_cursor_get_pos(uint32_t *x, uint32_t *y) {
  uint32_t pos = 0;

  /*__fail_return(out8(0x3D4, 0x0F));
  pos |= in8(0x3D5);

  __fail_return(out8(0x3D4, 0x0E));
  pos |= ((uint16_t)in8(0x3D5)) << 8;

  *x = pos % FB_WIDTH;
  *y = pos / FB_WIDTH;*/

  *x = fb_data.x;
  *y = fb_data.x;

  return true;
}

bool fb_cursor_set_pos(uint32_t x, uint32_t y) {
  if (x > FB_WIDTH) {
    x = 0;
    y++;
  }

  fb_data.x = x;
  fb_data.y = y > FB_HEIGHT ? 0 : y;

  __fb_cursor_update();
  return true;
}

void fb_write(uint8_t c) {
  switch (c) {
  case '\n':
    fb_data.x = 0;
    fb_data.y++;
    goto update;

  case '\r':
    fb_data.x = 0;
    goto update;

  case '\0':
    return;
  }

  uint32_t pos = __fb_pos();

  ((char *)FB_ADDR)[pos++] = c;
  ((char *)FB_ADDR)[pos]   = fb_data.color;

  if (++fb_data.x > FB_WIDTH - 1) {
    fb_data.y++;
    fb_data.x = 0;
  }

update:
  __fb_cursor_update();
}

uint8_t fb_fg_get() {
  uint8_t color = fb_data.color & 0x0F, i = 0;

  for (; i < __fb_color_count(); i++)
    if (fb_color_map[i] == color)
      return i;

  return 0;
}

void fb_fg_set(video_color_t c) {
  fb_data.color = (fb_data.color & 0xF0) | __fb_color(c);
}

uint8_t fb_bg_get() {
  uint8_t color = (fb_data.color & 0xF0) >> 4, i = 0;

  for (; i < __fb_color_count(); i++)
    if (fb_color_map[i] == color)
      return i;

  return 0;
}

void fb_bg_set(video_color_t c) {
  fb_data.color = (fb_data.color & 0x0F) | (__fb_color(c) << 4);
}

video_driver_t video_fb = {
    .init = fb_init,

    .clear = fb_clear,
    .write = fb_write,

    .fg_get = fb_fg_get,
    .fg_set = fb_fg_set,

    .bg_get = fb_bg_get,
    .bg_set = fb_bg_set,

    .cursor_hide = fb_cursor_hide,
    .cursor_show = fb_cursor_show,

    .cursor_get_pos = fb_cursor_get_pos,
    .cursor_set_pos = fb_cursor_set_pos,
};
