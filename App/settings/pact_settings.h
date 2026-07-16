/*
 * Pact MP-1 - user settings: tiny persisted struct + appliers.
 *
 * Storage goes through pact_io (device: "1:/pact.cfg", sim: a dotfile),
 * so the module is portable. UI edits the struct via ui_settings.c and
 * calls pact_settings_save(); platform code registers the brightness
 * applier (device: RM690B0 DCS 0x51, sim: no-op).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t brightness;   /* 0..11 - the Figma 12-segment bar */
    uint8_t albums_view;  /* 0 = carousel (08), 1 = grid (04) */
    uint8_t gapless;      /* 0/1 - engine set_next handoff on/off */
} pact_settings_t;

extern pact_settings_t pact_settings;

void pact_settings_init(const char *path);   /* load, else defaults */
void pact_settings_save(void);

/* level = 0..255 panel brightness, mapped from the 12-step bar */
void pact_settings_set_brightness_cb(void (*cb)(uint8_t level));
void pact_settings_apply_brightness(void);
