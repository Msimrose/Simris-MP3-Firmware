#include "pact_settings.h"
#include "../pact_io.h"
#include <stdio.h>
#include <string.h>

#define MAGIC   "PSET"
#define VERSION 1

pact_settings_t pact_settings = {
    .brightness  = 9,     /* bright-ish default, matches Figma mock */
    .albums_view = 0,     /* carousel */
    .gapless     = 1,
};

static char settings_path[128];
static void (*brightness_cb)(uint8_t level);

void pact_settings_init(const char *path)
{
    snprintf(settings_path, sizeof settings_path, "%s", path);

    pact_file_t *f = pact_open(path);
    if (!f) return;                       /* first boot: defaults stand */
    uint8_t hdr[5];
    pact_settings_t s;
    if (pact_read(f, hdr, 5) == 5 && memcmp(hdr, MAGIC, 4) == 0 &&
        hdr[4] == VERSION && pact_read(f, &s, sizeof s) == sizeof s) {
        if (s.brightness > 11) s.brightness = 11;
        s.albums_view = s.albums_view ? 1 : 0;
        s.gapless     = s.gapless ? 1 : 0;
        pact_settings = s;
    }
    pact_close(f);
}

void pact_settings_save(void)
{
    if (!settings_path[0]) return;
    pact_file_t *f = pact_open_write(settings_path);
    if (!f) return;
    uint8_t hdr[5] = { 'P', 'S', 'E', 'T', VERSION };
    pact_write(f, hdr, 5);
    pact_write(f, &pact_settings, sizeof pact_settings);
    pact_close(f);
}

void pact_settings_set_brightness_cb(void (*cb)(uint8_t level))
{
    brightness_cb = cb;
}

void pact_settings_apply_brightness(void)
{
    if (!brightness_cb) return;
    /* 12 steps -> 0..255, floor 40 so "min" is dim, never off */
    uint16_t lv = 40u + (uint16_t)pact_settings.brightness * (255u - 40u) / 11u;
    brightness_cb((uint8_t)lv);
}
