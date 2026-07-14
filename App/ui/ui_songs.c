/*
 * Pact MP-1 - Songs browse: every track in the library, alphabetical,
 * derived automatically by the library's song view.
 */
#include "ui_internal.h"
#include <stdlib.h>

#define SG_PITCH   42
#define SG_VISIBLE 9

static lv_obj_t **sg_rows;
static lv_obj_t **sg_names;
static lv_obj_t  *sg_list;
static size_t     sg_count;
static int        sg_sel, sg_top;

static void songs_paint(void)
{
    for (size_t i = 0; i < sg_count; i++) {
        bool sel = ((int)i == sg_sel);
        lv_obj_set_style_bg_opa(sg_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_font(sg_names[i],
                                   sel ? &diatype_medium_15 : &diatype_regular_15, 0);
    }
    if (sg_sel < sg_top) sg_top = sg_sel;
    if (sg_sel > sg_top + SG_VISIBLE - 1) sg_top = sg_sel - (SG_VISIBLE - 1);
    lv_obj_scroll_to_y(sg_list, sg_top * SG_PITCH, LV_ANIM_OFF);
}

void ui_songs_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (sg_sel < (int)sg_count - 1) { sg_sel++; songs_paint(); }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (sg_sel > 0) { sg_sel--; songs_paint(); }
        break;
    case PACT_EVT_CENTER:
        ui_play_track(ui_lib->songs[sg_sel]);
        ui_show_nowplaying();
        break;
    case PACT_EVT_UP:
        ui_show_menu();
        break;
    default: break;
    }
}

void ui_show_songs(void)
{
    ui_cur_screen = UI_SCR_SONGS;
    sg_count = ui_lib->count;

    free(sg_rows);
    free(sg_names);
    sg_rows = calloc(sg_count, sizeof(lv_obj_t *));
    sg_names = calloc(sg_count, sizeof(lv_obj_t *));

    lv_obj_t *scr = ui_screen_new();

    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Songs");
    lv_obj_set_style_text_font(hdr, &diatype_medium_18, 0);
    lv_obj_set_style_text_color(hdr, PACT_COL_TEXT, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 18);

    ui_battery_create_at(scr, 20);

    lv_obj_t *rule = lv_obj_create(scr);
    lv_obj_set_size(rule, 600, 1);
    lv_obj_set_pos(rule, 0, 52);
    lv_obj_set_style_bg_color(rule, PACT_COL_TEXT, 0);
    lv_obj_set_style_bg_opa(rule, PACT_OPA_RULE, 0);
    lv_obj_set_style_border_width(rule, 0, 0);

    sg_list = lv_obj_create(scr);
    lv_obj_set_size(sg_list, 600, 450 - 64);
    lv_obj_set_pos(sg_list, 0, 64);
    lv_obj_set_style_bg_opa(sg_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_list, 0, 0);
    lv_obj_set_style_pad_left(sg_list, 30, 0);
    lv_obj_set_style_pad_right(sg_list, 30, 0);
    lv_obj_set_style_pad_top(sg_list, 6, 0);
    lv_obj_set_flex_flow(sg_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sg_list, 8, 0);
    lv_obj_set_scrollbar_mode(sg_list, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < sg_count; i++) {
        const track_t *tr = &ui_lib->tracks[ui_lib->songs[i]];

        lv_obj_t *row = lv_obj_create(sg_list);
        lv_obj_set_size(row, lv_pct(100), 34);
        lv_obj_set_style_bg_color(row, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_left(row, 8, 0);
        lv_obj_set_style_pad_right(row, 14, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, tr->t.title);
        lv_obj_set_style_text_font(name, &diatype_regular_15, 0);
        lv_obj_set_style_text_color(name, PACT_COL_TEXT, 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_size(name, 280, 19);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *who = lv_label_create(row);
        lv_label_set_text(who, tr->t.artist[0] ? tr->t.artist : "(unknown)");
        lv_obj_set_style_text_font(who, &diatype_regular_11, 0);
        lv_obj_set_style_text_color(who, PACT_COL_TEXT_DIM, 0);
        lv_label_set_long_mode(who, LV_LABEL_LONG_DOT);
        lv_obj_set_size(who, 160, 15);
        lv_obj_align(who, LV_ALIGN_LEFT_MID, 300, 0);

        lv_obj_t *dur = lv_label_create(row);
        lv_label_set_text_fmt(dur, "%u:%02u", tr->t.duration_ms / 60000,
                              (tr->t.duration_ms / 1000) % 60);
        lv_obj_set_style_text_font(dur, &diatype_regular_11, 0);
        lv_obj_set_style_text_color(dur, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(dur, LV_ALIGN_RIGHT_MID, 0, 0);

        sg_rows[i] = row;
        sg_names[i] = name;
    }

    if (sg_sel >= (int)sg_count) sg_sel = 0;
    sg_top = 0;
    songs_paint();
    ui_bind_keys(sg_list);
    ui_screen_show(scr);
}
