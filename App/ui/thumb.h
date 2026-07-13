/* Pact MP-1 - thumbnail loader: uncompressed BMP (24/32-bit) -> ARGB8888
 * LVGL draw buffer, read through pact_io. Raw RAM buffers are the only
 * image source LVGL can scale-transform, which the carousel needs; on
 * device the thumbnail cache stores exactly this kind of pre-scaled BMP. */
#pragma once
#include "lvgl.h"

lv_draw_buf_t *pact_thumb_load_bmp(const char *path);  /* NULL on failure */
