/* Pact MP-1 - UI entry points. LVGL is called only from the UI task/thread. */
#pragma once
#include "lvgl.h"
#include "../input/input_events.h"
#include "../library/library.h"

/* Returns an LVGL image source for an album's cover (e.g. "A:/path/thumb.jpg")
 * or NULL if the album has no art. The provider owns the returned string
 * until the next call. */
typedef const char *(*ui_art_provider_t)(size_t album_idx);

void        ui_init(void);
lv_group_t *ui_group(void);           /* input group the keypad indev feeds  */
void        ui_handle_event(pact_event_t evt);
void        ui_set_battery(int percent, bool charging);

void        ui_set_library(const library_t *lib, ui_art_provider_t art);
void        ui_set_on_play(void (*fn)(const char *path));
