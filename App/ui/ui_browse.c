/*
 * Pact MP-1 - browse screens: Albums (cover rows) and album track list.
 * Plain list styling for now; layout values get re-cut to the Figma design
 * once card direction is decided.
 */
#include "ui_internal.h"
#include <stdio.h>
#include <stdlib.h>

#define PAD_X   48
#define ROW_H   72   /* albums: 56px cover + breathing room */
#define TROW_H  46

/* ---- albums ------------------------------------------------------------ */

static lv_obj_t **alb_rows;
static lv_obj_t **alb_names;
static size_t     alb_count;
static int        alb_sel;

static void albums_paint(void)
{
    for (size_t i = 0; i < alb_count; i++) {
        bool sel = ((int)i == alb_sel);
        lv_obj_set_style_bg_opa(alb_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(alb_names[i],
                                    sel ? PACT_COL_TEXT : PACT_COL_TEXT_DIM, 0);
    }
    lv_obj_scroll_to_view(alb_rows[alb_sel], LV_ANIM_ON);
}

void ui_albums_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (alb_sel < (int)alb_count - 1) { alb_sel++; albums_paint(); }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (alb_sel > 0) { alb_sel--; albums_paint(); }
        break;
    case PACT_EVT_CENTER:
        ui_show_tracks((size_t)alb_sel);
        break;
    case PACT_EVT_UP:
        ui_show_menu();
        break;
    default: break;
    }
}

void ui_show_albums(void)
{
    ui_cur_screen = UI_SCR_ALBUMS;
    alb_count = ui_lib->album_count;

    free(alb_rows);
    free(alb_names);
    alb_rows = calloc(alb_count, sizeof(lv_obj_t *));
    alb_names = calloc(alb_count, sizeof(lv_obj_t *));

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, PACT_COL_GROUND, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Albums");
    lv_obj_set_style_text_font(hdr, pact_font_data, 0);
    lv_obj_set_style_text_color(hdr, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_text_letter_space(hdr, 2, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, PAD_X, 20);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 600, 450 - 56);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_left(list, PAD_X - 16, 0);
    lv_obj_set_style_pad_right(list, PAD_X - 16, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < alb_count; i++) {
        const album_t *al = &ui_lib->albums[i];

        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), ROW_H);
        lv_obj_set_style_bg_color(row, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_left(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        const char *art = ui_art_provider ? ui_art_provider(i) : NULL;
        if (art) {
            lv_obj_t *cov = lv_image_create(row);
            lv_image_set_src(cov, art);
            lv_obj_set_size(cov, 56, 56);
            lv_image_set_inner_align(cov, LV_IMAGE_ALIGN_STRETCH);
            lv_obj_align(cov, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_radius(cov, 4, 0);
            lv_obj_set_style_clip_corner(cov, true, 0);
        }

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, al->album[0] ? al->album : "(unknown album)");
        lv_obj_set_style_text_font(name, &diatype_regular_22, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 56 + 16, -12);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, 600 - PAD_X * 2 - 56 - 24);

        lv_obj_t *sub = lv_label_create(row);
        lv_label_set_text_fmt(sub, "%s  ·  %zu tracks",
                              al->artist[0] ? al->artist : "(unknown)", al->count);
        lv_obj_set_style_text_font(sub, &diatype_regular_16, 0);
        lv_obj_set_style_text_color(sub, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(sub, LV_ALIGN_LEFT_MID, 56 + 16, 14);

        alb_rows[i] = row;
        alb_names[i] = name;
    }

    if (alb_sel >= (int)alb_count) alb_sel = 0;
    albums_paint();
    ui_bind_keys(list);
    lv_screen_load(scr);
}

/* ---- track list -------------------------------------------------------- */

static lv_obj_t **trk_rows;
static lv_obj_t **trk_names;
static size_t     trk_count;
static size_t     trk_first;
static size_t     trk_album;
static int        trk_sel;

static void tracks_paint(void)
{
    for (size_t i = 0; i < trk_count; i++) {
        bool sel = ((int)i == trk_sel);
        lv_obj_set_style_bg_opa(trk_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(trk_names[i],
                                    sel ? PACT_COL_TEXT : PACT_COL_TEXT_DIM, 0);
    }
    lv_obj_scroll_to_view(trk_rows[trk_sel], LV_ANIM_ON);
}

void ui_tracks_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (trk_sel < (int)trk_count - 1) { trk_sel++; tracks_paint(); }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (trk_sel > 0) { trk_sel--; tracks_paint(); }
        break;
    case PACT_EVT_CENTER:
        if (ui_on_play)
            ui_on_play(ui_lib->tracks[trk_first + trk_sel].path);
        break;
    case PACT_EVT_UP:
        ui_show_albums();
        break;
    default: break;
    }
}

void ui_show_tracks(size_t album_idx)
{
    ui_cur_screen = UI_SCR_TRACKS;
    const album_t *al = &ui_lib->albums[album_idx];
    trk_album = album_idx;
    trk_first = al->first;
    trk_count = al->count;
    trk_sel = 0;

    free(trk_rows);
    free(trk_names);
    trk_rows = calloc(trk_count, sizeof(lv_obj_t *));
    trk_names = calloc(trk_count, sizeof(lv_obj_t *));

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, PACT_COL_GROUND, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* header: cover + album + artist */
    const char *art = ui_art_provider ? ui_art_provider(album_idx) : NULL;
    if (art) {
        lv_obj_t *cov = lv_image_create(scr);
        lv_image_set_src(cov, art);
        lv_obj_set_size(cov, 64, 64);
        lv_image_set_inner_align(cov, LV_IMAGE_ALIGN_STRETCH);
        lv_obj_align(cov, LV_ALIGN_TOP_LEFT, PAD_X, 16);
        lv_obj_set_style_radius(cov, 4, 0);
        lv_obj_set_style_clip_corner(cov, true, 0);
    }
    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, al->album[0] ? al->album : "(unknown album)");
    lv_obj_set_style_text_font(hdr, &diatype_regular_24, 0);
    lv_obj_set_style_text_color(hdr, PACT_COL_TEXT, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, PAD_X + 64 + 16, 22);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, al->artist[0] ? al->artist : "(unknown)");
    lv_obj_set_style_text_font(sub, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(sub, PACT_COL_TEXT_DIM, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, PAD_X + 64 + 16, 54);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 600, 450 - 96);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_left(list, PAD_X - 16, 0);
    lv_obj_set_style_pad_right(list, PAD_X - 16, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 2, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < trk_count; i++) {
        const track_t *tr = &ui_lib->tracks[trk_first + i];

        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), TROW_H);
        lv_obj_set_style_bg_color(row, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_left(row, 16, 0);
        lv_obj_set_style_pad_right(row, 16, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *no = lv_label_create(row);
        lv_label_set_text_fmt(no, "%u", tr->t.track_no);
        lv_obj_set_style_text_font(no, &diatype_regular_16, 0);
        lv_obj_set_style_text_color(no, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(no, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, tr->t.title);
        lv_obj_set_style_text_font(name, &diatype_regular_22, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 32, 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, 600 - PAD_X * 2 - 32 - 72);

        lv_obj_t *dur = lv_label_create(row);
        lv_label_set_text_fmt(dur, "%u:%02u", tr->t.duration_ms / 60000,
                              (tr->t.duration_ms / 1000) % 60);
        lv_obj_set_style_text_font(dur, &diatype_regular_16, 0);
        lv_obj_set_style_text_color(dur, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(dur, LV_ALIGN_RIGHT_MID, 0, 0);

        trk_rows[i] = row;
        trk_names[i] = name;
    }

    tracks_paint();
    ui_bind_keys(list);
    lv_screen_load(scr);
}
