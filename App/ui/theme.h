/*
 * Pact MP-1 - brand tokens (docs/brand-ui-system.md) + the exact Diatype
 * cuts used by the Figma design (file AN66SqmZfUbMRvjufRK1YR).
 *
 * Monochrome by design: color exists only in album art and the battery fill.
 */
#pragma once
#include "lvgl.h"

#define PACT_COL_GROUND    lv_color_hex(0x000000) /* true black - AMOLED off  */
#define PACT_COL_TEXT      lv_color_hex(0xF4F3EF) /* warm white               */
#define PACT_COL_TEXT_DIM  lv_color_hex(0x8A887F) /* secondary grey           */
#define PACT_COL_SELECT    lv_color_hex(0x1A1A1A) /* subtle selection fill    */
#define PACT_COL_WHITE     lv_color_hex(0xFFFFFF) /* progress bar / LED mirror */
#define PACT_COL_BATT_LOW  lv_color_hex(0xE04030) /* battery <15% - the ONLY red */

/* white-on-black hairlines/tracks are warm white at fixed opacities */
#define PACT_OPA_RULE    31   /* 12% - header hairline        */
#define PACT_OPA_TRACK   41   /* 16% - progress track         */
#define PACT_OPA_DOT     51   /* 20% - inactive carousel dot  */
#define PACT_OPA_NEAR   140   /* 55% - carousel near neighbor */
#define PACT_OPA_FAR     77   /* 30% - carousel far neighbor  */

/* Exact cuts from the Figma frames (menu 01 / NP-A / rail 08 / tracks 05) */
LV_FONT_DECLARE(diatype_regular_11);
LV_FONT_DECLARE(diatype_regular_14);
LV_FONT_DECLARE(diatype_regular_15);
LV_FONT_DECLARE(diatype_regular_19);
LV_FONT_DECLARE(diatype_regular_20);  /* includes the chevron glyph */
LV_FONT_DECLARE(diatype_medium_15);
LV_FONT_DECLARE(diatype_medium_16);
LV_FONT_DECLARE(diatype_medium_18);
LV_FONT_DECLARE(diatype_medium_19);
LV_FONT_DECLARE(diatype_medium_22);
LV_FONT_DECLARE(diatype_medium_27);
LV_FONT_DECLARE(diatype_light_13);

/* Legacy cuts (pre-spec screens; prune once nothing references them) */
LV_FONT_DECLARE(diatype_regular_16);
LV_FONT_DECLARE(diatype_regular_22);
LV_FONT_DECLARE(diatype_regular_24);
LV_FONT_DECLARE(diatype_medium_32);
LV_FONT_DECLARE(scotch_mono_16);
LV_FONT_DECLARE(slab_mono_16);

extern const lv_font_t *pact_font_data;   /* sim --datafont trial hook */

/* Brand wordmark, 94x12, baked from the Figma asset */
LV_IMAGE_DECLARE(simris_logo);


#define PACT_ANIM_MS 180
