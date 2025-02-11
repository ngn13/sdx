#pragma once

#include "util/printk.h"
#include "types.h"

#define video_fail(f, ...) pfail("Video: (%s) " f, NULL == video_current ? "none" : video_current->name, ##__VA_ARGS__)
#define video_info(f, ...) pinfo("Video: (%s) " f, NULL == video_current ? "none" : video_current->name, ##__VA_ARGS__)
#define video_debg(f, ...) pdebg("Video: (%s) " f, NULL == video_current ? "none" : video_current->name, ##__VA_ARGS__)

typedef enum video_mode {
  VIDEO_MODE_NONE        = 0,
  VIDEO_MODE_FRAMEBUFFER = 1,
} video_mode_t;

typedef enum video_color {
  VIDEO_COLOR_BLACK       = 0,
  VIDEO_COLOR_WHITE       = 1,
  VIDEO_COLOR_BLUE        = 2,
  VIDEO_COLOR_GREEN       = 3,
  VIDEO_COLOR_RED         = 4,
  VIDEO_COLOR_BROWN       = 5,
  VIDEO_COLOR_YELLOW      = 6,
  VIDEO_COLOR_LIGHT_BLUE  = 7,
  VIDEO_COLOR_LIGHT_GREEN = 8,
  VIDEO_COLOR_LIGHT_RED   = 9,
} video_color_t;

typedef struct video_driver {
  const char *name;  // driver's name
  int32_t (*init)(); // initializes the video driver

  void (*clear)();        // clears the current video graphics
  void (*write)(uint8_t); // writes a single char to the screen

  void (*fg_set)(video_color_t); // set the foreground color
  uint8_t (*fg_get)();           // get the foreground color
  void (*bg_set)(video_color_t); // set the background color
  uint8_t (*bg_get)();           // set the background color

  int32_t (*cursor_show)();                            // shows the cursor
  int32_t (*cursor_hide)();                            // hides the cursor
  int32_t (*cursor_get_pos)(uint32_t *x, uint32_t *y); // get the cursor position
  int32_t (*cursor_set_pos)(uint32_t x, uint32_t y);   // set the cursor position
} video_driver_t;

extern video_driver_t *video_current; // current selected video driver
extern video_driver_t  video_fb;      // framebuffer video driver

// video functions
int32_t      video_init(video_mode_t mode);
video_mode_t video_mode(); // returns the current video mode

void video_clear();
void video_write(uint8_t c);

void    video_fg_set(video_color_t c);
uint8_t video_fg_get();

void    video_bg_set(video_color_t c);
uint8_t video_bg_get();

int32_t video_cursor_show();
int32_t video_cursor_hide();

int32_t video_cursor_get_pos(uint32_t *x, uint32_t *y);
int32_t video_cursor_set_pos(uint32_t x, uint32_t y);
