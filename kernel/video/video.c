#include "video.h"

// current selected video driver
video_driver_t *cur = NULL;

// video driver list
video_driver_t *video_drivers[] = {
    NULL,      // VIDEO_MODE_NONE
    &video_fb, // VIDEO_MODE_FRAMEBUFFER
};

#define __video_driver_count() sizeof(video_drivers) / sizeof(video_driver_t *)
#define __video_get_driver(m)  video_drivers[m]
#define __video_driver_check(x)                                                                                        \
  if (NULL == cur)                                                                                                     \
  return x

bool video_init(video_mode_t mode) {
  if (mode > __video_driver_count())
    return false;

  cur = __video_get_driver(mode);

  if (!cur->init())
    return false;

  cur->bg_set(VIDEO_COLOR_BLACK);
  cur->fg_set(VIDEO_COLOR_WHITE);

  return true;
}

video_mode_t video_mode() {
  for (uint8_t i = 0; i < __video_driver_count(); i++)
    if (cur == __video_get_driver(i))
      return i;
  return 0;
}

void video_clear() {
  __video_driver_check();
  cur->clear();
}

void video_write(uint8_t c) {
  __video_driver_check();
  cur->write(c);
}

void video_fg_set(video_color_t c) {
  __video_driver_check();
  cur->fg_set(c);
}

uint8_t video_fg_get() {
  __video_driver_check(0);
  return cur->fg_get();
}

void video_bg_set(video_color_t c) {
  __video_driver_check();
  cur->bg_set(c);
}

uint8_t video_bg_get() {
  __video_driver_check(0);
  return cur->bg_get();
}

bool video_cursor_show() {
  __video_driver_check(false);
  return cur->cursor_show();
}

bool video_cursor_hide() {
  __video_driver_check(false);
  return cur->cursor_hide();
}

bool video_cursor_get_pos(uint32_t *x, uint32_t *y) {
  __video_driver_check(false);
  return cur->cursor_get_pos(x, y);
}

bool video_cursor_set_pos(uint32_t x, uint32_t y) {
  __video_driver_check(false);
  return cur->cursor_set_pos(x, y);
}
