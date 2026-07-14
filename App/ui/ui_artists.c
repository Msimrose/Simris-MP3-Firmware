/*
 * Pact MP-1 - Artists browse: list of artists (derived automatically from
 * file tags by the library), and per-artist page (their albums).
 * Type follows the exact-spec cuts; layout in the spirit of Figma 09.
 */
#include "ui_internal.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- artists list ------------------------------------------------------- */

#define AR_PITCH   45   /* 43px row + 2 gap, same rhythm as the menu */
#define AR_VISIBLE 8

static lv_obj_t **ar_rows;
static lv_obj_t **ar_names;
static lv_obj_t  *ar_list;
static size_t     ar_count;
static int        ar_sel, ar_top;

static void artists_paint(void)
{
    for (size_t i = 0; i < ar_count; i++) {
        bool sel = ((int)i == ar_sel);
        lv_obj_set_style_bg_opa(ar_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_font(ar_names[i],
                                   sel ? &diatype_medium_19 : &diatype_regular_19, 0);
    }
    if (ar_sel < ar_top) ar_top = ar_sel;
    if (ar_sel > ar_top + AR_VISIBLE - 1) ar_top = ar_sel - (AR_VISIBLE - 1);
    lv_obj_scroll_to_y(ar_list, ar_top * AR_PITCH, LV_ANIM_OFF);
}

void ui_artists_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (ar_sel < (int)ar_count - 1) { ar_sel++; artists_paint(); }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (ar_sel > 0) { ar_sel--; artists_paint(); }
        break;
    case PACT_EVT_CENTER:
        ui_show_artist((size_t)ar_sel);
        break;
    case PACT_EVT_UP:
        ui_show_menu();
        break;
    default: break;
    }
}

void ui_show_artists(void)
{
    ui_cur_screen = UI_SCR_ARTISTS;
    ar_count = ui_lib->artist_count;

    free(ar_rows);
    free(ar_names);
    ar_rows = calloc(ar_count, sizeof(lv_obj_t *));
    ar_names = calloc(ar_count, sizeof(lv_obj_t *));

    lv_obj_t *scr = ui_screen_new();

    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Artists");
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

    ar_list = lv_obj_create(scr);
    lv_obj_set_size(ar_list, 600, 450 - 72);
    lv_obj_set_pos(ar_list, 0, 72);
    lv_obj_set_style_bg_opa(ar_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ar_list, 0, 0);
    lv_obj_set_style_pad_all(ar_list, 0, 0);
    lv_obj_set_flex_flow(ar_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ar_list, 2, 0);
    lv_obj_set_scrollbar_mode(ar_list, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < ar_count; i++) {
        const artist_t *ar = &ui_lib->artists[i];

        lv_obj_t *row = lv_obj_create(ar_list);
        lv_obj_set_size(row, lv_pct(100), 43);
        lv_obj_set_style_bg_color(row, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_left(row, 22, 0);
        lv_obj_set_style_pad_right(row, 18, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, ar->name[0] ? ar->name : "(unknown)");
        lv_obj_set_style_text_font(lbl, &diatype_regular_19, 0);
        lv_obj_set_style_text_color(lbl, PACT_COL_TEXT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_size(lbl, 360, 24);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *meta = lv_label_create(row);
        lv_label_set_text_fmt(meta, "%zu %s", ar->track_count,
                              ar->track_count == 1 ? "song" : "songs");
        lv_obj_set_style_text_font(meta, &diatype_regular_14, 0);
        lv_obj_set_style_text_color(meta, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(meta, LV_ALIGN_RIGHT_MID, -18, 0);

        lv_obj_t *chev = lv_label_create(row);
        lv_label_set_text(chev, "\xE2\x80\xBA");
        lv_obj_set_style_text_font(chev, &diatype_regular_20, 0);
        lv_obj_set_style_text_color(chev, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);

        ar_rows[i] = row;
        ar_names[i] = lbl;
    }

    if (ar_sel >= (int)ar_count) ar_sel = 0;
    ar_top = 0;
    artists_paint();
    ui_bind_keys(ar_list);
    ui_screen_show(scr);
}

/* ---- artist page (their albums) ----------------------------------------- */

#define AA_PITCH   72
#define AA_VISIBLE 4

static lv_obj_t **aa_rows;
static lv_obj_t **aa_names;
static lv_obj_t  *aa_list;
static size_t     aa_count;
static size_t     cur_artist;
static int        aa_sel, aa_top;

static void artist_paint(void)
{
    for (size_t i = 0; i < aa_count; i++) {
        bool sel = ((int)i == aa_sel);
        lv_obj_set_style_bg_opa(aa_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(aa_names[i],
                                    sel ? PACT_COL_TEXT : PACT_COL_TEXT_DIM, 0);
    }
    if (aa_sel < aa_top) aa_top = aa_sel;
    if (aa_sel > aa_top + AA_VISIBLE - 1) aa_top = aa_sel - (AA_VISIBLE - 1);
    lv_obj_scroll_to_y(aa_list, aa_top * AA_PITCH, LV_ANIM_OFF);
}

void ui_artist_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (aa_sel < (int)aa_count - 1) { aa_sel++; artist_paint(); }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (aa_sel > 0) { aa_sel--; artist_paint(); }
        break;
    case PACT_EVT_CENTER:
        ui_tracks_back = UI_SCR_ARTIST;
        ui_tracks_back_artist = cur_artist;
        ui_show_tracks(ui_lib->artists[cur_artist].albums[aa_sel]);
        break;
    case PACT_EVT_UP:
        ui_show_artists();
        break;
    default: break;
    }
}

void ui_show_artist(size_t artist_idx)
{
    ui_cur_screen = UI_SCR_ARTIST;
    cur_artist = artist_idx;
    const artist_t *ar = &ui_lib->artists[artist_idx];
    aa_count = ar->album_count;

    free(aa_rows);
    free(aa_names);
    aa_rows = calloc(aa_count, sizeof(lv_obj_t *));
    aa_names = calloc(aa_count, sizeof(lv_obj_t *));

    lv_obj_t *scr = ui_screen_new();
    ui_battery_create_at(scr, 22);

    /* header: artist name + derived meta, Figma-09 typography */
    lv_obj_t *name = lv_label_create(scr);
    lv_label_set_text(name, ar->name[0] ? ar->name : "(unknown)");
    lv_obj_set_style_text_font(name, &diatype_medium_27, 0);
    lv_obj_set_style_text_color(name, PACT_COL_TEXT, 0);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_size(name, 440, 34);
    lv_obj_set_pos(name, 40, 36);

    lv_obj_t *meta = lv_label_create(scr);
    lv_label_set_text_fmt(meta, "%zu %s · %zu %s",
                          ar->album_count, ar->album_count == 1 ? "ALBUM" : "ALBUMS",
                          ar->track_count, ar->track_count == 1 ? "SONG" : "SONGS");
    lv_obj_set_style_text_font(meta, &diatype_regular_11, 0);
    lv_obj_set_style_text_color(meta, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_text_letter_space(meta, 1, 0);
    lv_obj_set_pos(meta, 40, 76);

    aa_list = lv_obj_create(scr);
    lv_obj_set_size(aa_list, 600 - 80, 450 - 110);
    lv_obj_set_pos(aa_list, 40, 110);
    lv_obj_set_style_bg_opa(aa_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(aa_list, 0, 0);
    lv_obj_set_style_pad_all(aa_list, 0, 0);
    lv_obj_set_flex_flow(aa_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(aa_list, 8, 0);
    lv_obj_set_scrollbar_mode(aa_list, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < aa_count; i++) {
        const album_t *al = &ui_lib->albums[ar->albums[i]];

        lv_obj_t *row = lv_obj_create(aa_list);
        lv_obj_set_size(row, lv_pct(100), 64);
        lv_obj_set_style_bg_color(row, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_left(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        const char *art = ui_art_provider ? ui_art_provider(ar->albums[i], 56) : NULL;
        if (art) {
            lv_obj_t *cov = lv_image_create(row);
            lv_image_set_src(cov, art);
            lv_obj_set_size(cov, 56, 56);
            lv_obj_align(cov, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_radius(cov, 4, 0);
            lv_obj_set_style_clip_corner(cov, true, 0);
        }

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, al->album[0] ? al->album : "(unknown album)");
        lv_obj_set_style_text_font(lbl, &diatype_regular_15, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_size(lbl, 340, 20);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 56 + 14, -10);

        lv_obj_t *cnt = lv_label_create(row);
        lv_label_set_text_fmt(cnt, "%zu tracks", al->count);
        lv_obj_set_style_text_font(cnt, &diatype_regular_11, 0);
        lv_obj_set_style_text_color(cnt, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(cnt, LV_ALIGN_LEFT_MID, 56 + 14, 12);

        aa_rows[i] = row;
        aa_names[i] = lbl;
    }

    aa_sel = 0;
    aa_top = 0;
    artist_paint();
    ui_bind_keys(aa_list);
    ui_screen_show(scr);
}
