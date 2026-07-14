#pragma once
#include "lvgl.h"
#include <stdbool.h>

lv_display_t *pact_sdl_display_create(int32_t w, int32_t h, bool big);
lv_indev_t   *pact_sdl_keyboard_create(void);
