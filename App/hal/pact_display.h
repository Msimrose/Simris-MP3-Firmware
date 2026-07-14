/*
 * Pact MP-1 - display driver interface (RM690B0 QSPI AMOLED).
 *
 * Device implementation: App/hal/disp_rm690b0.c. Powers the TPS65632
 * panel rails, runs the official Startek init, registers the LVGL
 * display (600x450 landscape) with its flush callback. The simulator
 * uses the SDL driver (sim/sdl_driver.c) instead.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Full panel bring-up: rails -> reset -> init -> LVGL display registered.
 * Call from the UI task after lv_init(). Returns false on QSPI failure. */
bool pact_display_init(void);

void pact_display_set_brightness(uint8_t level);   /* 0..255, DCS 0x51 */

/* Panel off (DCS 0x28 + sleep-in 0x10), then rails down + reset low.
 * After this, pact_display_init() is required to light up again. */
void pact_display_off(void);
