/*
 * Settings screen - Figma 07 (node 16:53), plus one added row: Albums View
 * (carousel/grid), per Micah 2026-07-15. Geometry from the Figma frame:
 * title at (38,26), rows on a 42px pitch, 1px hairline separators, values
 * right-aligned to x=562, brightness = twelve 6x10 segments on a 9px pitch.
 *
 * Live rows: Gapless (engine handoff on/off), Brightness (center = edit,
 * wheel adjusts + applies live), Albums View. The remaining Figma rows
 * (Crossfade / EQ / Volume Limit / Sleep Timer / Theme / About) render
 * faithfully but are inert until their features exist - values dimmed.
 */
#include "ui_internal.h"
#include "../settings/pact_settings.h"
#include "../audio/audio_engine.h"

#define ROW_Y0     68
#define ROW_PITCH  42
#define ROW_LX     38
#define ROW_RX     562
#define SEG_N      12

typedef enum { ROW_GAPLESS, ROW_STATIC, ROW_BRIGHT, ROW_VIEW, ROW_ABOUT } row_kind_t;

static const struct { const char *label; row_kind_t kind; const char *fixed; }
rows[] = {
    { "Gapless Playback", ROW_GAPLESS, NULL },
    { "Crossfade",        ROW_STATIC,  "Off" },
    { "EQ",               ROW_STATIC,  "Flat" },
    { "Volume Limit",     ROW_STATIC,  "Off" },
    { "Brightness",       ROW_BRIGHT,  NULL },
    { "Sleep Timer",      ROW_STATIC,  "Off" },
    { "Theme",            ROW_STATIC,  "Dark" },
    { "Albums View",      ROW_VIEW,    NULL },
    { "About",            ROW_ABOUT,   "›" },
};
#define ROW_COUNT ((int)(sizeof rows / sizeof rows[0]))

static int       sel;
static bool      editing;              /* brightness edit mode */
static lv_obj_t *hilite;
static lv_obj_t *value_lbl[ROW_COUNT];
static lv_obj_t *segs[SEG_N];

static const char *value_text(int i)
{
    switch (rows[i].kind) {
    case ROW_GAPLESS: return pact_settings.gapless ? "On" : "Off";
    case ROW_VIEW:    return pact_settings.albums_view ? "Grid" : "Carousel";
    default:          return rows[i].fixed;
    }
}

static void paint_row_values(void)
{
    for (int i = 0; i < ROW_COUNT; i++) {
        if (!value_lbl[i]) continue;
        lv_label_set_text(value_lbl[i], value_text(i));
        bool live = rows[i].kind == ROW_GAPLESS || rows[i].kind == ROW_VIEW;
        lv_obj_set_style_text_color(value_lbl[i],
            live ? PACT_COL_TEXT : PACT_COL_TEXT_DIM, 0);
        lv_obj_align(value_lbl[i], LV_ALIGN_TOP_RIGHT,
                     -(600 - ROW_RX), ROW_Y0 + i * ROW_PITCH + 2);
    }
    for (int s = 0; s < SEG_N; s++) {
        bool on = s <= pact_settings.brightness;
        lv_obj_set_style_bg_color(segs[s], PACT_COL_WHITE, 0);
        lv_obj_set_style_bg_opa(segs[s],
            on ? (editing ? LV_OPA_COVER : 200) : PACT_OPA_TRACK, 0);
    }
}

static void paint_selection(void)
{
    lv_obj_set_y(hilite, ROW_Y0 + sel * ROW_PITCH - 7);
    lv_obj_set_style_border_opa(hilite, editing ? 90 : 0, 0);
}

void ui_show_settings(void)
{
    ui_cur_screen = UI_SCR_SETTINGS;
    editing = false;
    lv_obj_t *scr = ui_screen_new();

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &diatype_medium_18, 0);
    lv_obj_set_style_text_color(title, PACT_COL_TEXT, 0);
    lv_obj_set_pos(title, ROW_LX, 26);
    ui_battery_create(scr);

    hilite = lv_obj_create(scr);
    lv_obj_set_size(hilite, 540, 34);
    lv_obj_set_x(hilite, 30);
    lv_obj_set_style_bg_color(hilite, PACT_COL_SELECT, 0);
    lv_obj_set_style_bg_opa(hilite, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hilite, 6, 0);
    lv_obj_set_style_border_width(hilite, 1, 0);
    lv_obj_set_style_border_color(hilite, PACT_COL_TEXT, 0);
    lv_obj_set_style_border_opa(hilite, 0, 0);

    for (int i = 0; i < ROW_COUNT; i++) {
        int y = ROW_Y0 + i * ROW_PITCH;

        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, rows[i].label);
        lv_obj_set_style_text_font(lbl, &diatype_medium_16, 0);
        lv_obj_set_style_text_color(lbl, PACT_COL_TEXT, 0);
        lv_obj_set_pos(lbl, ROW_LX, y);

        if (rows[i].kind == ROW_BRIGHT) {
            for (int s = 0; s < SEG_N; s++) {
                lv_obj_t *sg = lv_obj_create(scr);
                lv_obj_set_size(sg, 6, 10);
                lv_obj_set_pos(sg, 454 + s * 9, y + 5);
                lv_obj_set_style_radius(sg, 1, 0);
                lv_obj_set_style_border_width(sg, 0, 0);
                segs[s] = sg;
            }
            value_lbl[i] = NULL;
        } else {
            lv_obj_t *val = lv_label_create(scr);
            lv_obj_set_style_text_font(val, &diatype_regular_15, 0);
            value_lbl[i] = val;
        }

        if (i < ROW_COUNT - 1) {
            lv_obj_t *rule = lv_obj_create(scr);
            lv_obj_set_size(rule, 524, 1);
            lv_obj_set_pos(rule, ROW_LX, y + 30);
            lv_obj_set_style_bg_color(rule, PACT_COL_TEXT, 0);
            lv_obj_set_style_bg_opa(rule, PACT_OPA_RULE, 0);
            lv_obj_set_style_border_width(rule, 0, 0);
        }
    }


    /* invisible key sink: routes wheel/buttons to this screen's handler */
    lv_obj_t *sink = lv_obj_create(scr);
    lv_obj_set_size(sink, 1, 1);
    lv_obj_set_style_bg_opa(sink, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sink, 0, 0);
    ui_bind_keys(sink);

    paint_row_values();
    paint_selection();
    ui_screen_show(scr);
}

void ui_settings_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (editing) {
            if (pact_settings.brightness < SEG_N - 1) pact_settings.brightness++;
            pact_settings_apply_brightness();
            paint_row_values();
        } else if (sel < ROW_COUNT - 1) {
            sel++;
            paint_selection();
        }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (editing) {
            if (pact_settings.brightness > 0) pact_settings.brightness--;
            pact_settings_apply_brightness();
            paint_row_values();
        } else if (sel > 0) {
            sel--;
            paint_selection();
        }
        break;
    case PACT_EVT_CENTER:
        if (rows[sel].kind == ROW_GAPLESS) {
            pact_settings.gapless ^= 1;
            if (!pact_settings.gapless) audio_engine_set_next(NULL);
            pact_settings_save();
            paint_row_values();
        } else if (rows[sel].kind == ROW_VIEW) {
            pact_settings.albums_view ^= 1;
            pact_settings_save();
            paint_row_values();
        } else if (rows[sel].kind == ROW_BRIGHT) {
            editing = !editing;
            if (!editing) pact_settings_save();
            paint_selection();
            paint_row_values();
        }
        break;
    case PACT_EVT_UP:
        if (editing) {
            editing = false;
            pact_settings_save();
            paint_selection();
            paint_row_values();
        } else {
            ui_show_menu();
        }
        break;
    default: break;
    }
}
