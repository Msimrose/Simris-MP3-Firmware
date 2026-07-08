/* Pact MP-1 - UI entry points. LVGL is called only from the UI task/thread. */
#pragma once
#include "lvgl.h"
#include "../input/input_events.h"

void        ui_init(void);
lv_group_t *ui_group(void);           /* input group the keypad indev feeds  */
void        ui_handle_event(pact_event_t evt);
void        ui_set_battery(int percent, bool charging);
