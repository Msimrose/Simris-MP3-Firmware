/*
 * Pact MP-1 - on-device album-art thumb cache.
 *
 * Built by storage_task after the library scan: for every album with art,
 * decode the source JPEG once (TJPGD, CPU - happens once per album ever)
 * and write pre-scaled 24-bit BMPs, one per UI size, keyed by the album's
 * first-track path hash (same keying as the sim's sips pipeline):
 *
 *     <dir>/<hash8>_<px>.bmp        e.g. 1:/.pactart/1a2b3c4d_190.bmp
 *
 * BMP everywhere because both consumers already read it: lv_image via the
 * LVGL BMP decoder + FatFs driver ("A:" paths), and the carousel via
 * pact_thumb_load_bmp (which swaps the provider path's extension to .bmp -
 * a no-op here). The HW JPEG peripheral stays reserved for video (Phase 9);
 * this module is portable C, verified on host by sim/fatfs_test.
 *
 * Known limits (skipped gracefully, thumb just doesn't exist): progressive
 * JPEG sources (TJPGD and the H7 HW codec both reject them - the desktop
 * sync tool should transcode), PNG folder art (TODO: lodepng).
 */
#pragma once
#include "library.h"
#include <stdbool.h>
#include <stddef.h>

/* Every size the UI requests: browse/artist rows, album header, carousel +
 * menu pane, now-playing. Keep in sync with the ui_art_provider callers. */
#define PACT_THUMB_SIZE_COUNT 5
extern const int pact_thumb_sizes[PACT_THUMB_SIZE_COUNT];

/* Build all missing thumbs; returns the number of BMP files written.
 * Safe to re-run (existing files are skipped). Creates dir if needed. */
size_t thumbcache_build(const library_t *lib, const char *dir);

/* Fill buf with the thumb path for (album, px) and return buf if the file
 * exists, else NULL. */
const char *thumbcache_file(const library_t *lib, size_t album_idx, int px,
                            const char *dir, char *buf, size_t buflen);
