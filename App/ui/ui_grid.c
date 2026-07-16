/*
 * Albums grid - Figma 04 "Library (grid)" (node 13:2): 4x2 pages of 116px
 * covers on a 136px pitch from (38,70), selected album named bottom-left
 * (name + dim artist). An alternative to the carousel, chosen in
 * Settings -> Albums View. Selection = the selected tile at full
 * opacity, all others dimmed (Figma treatment, like the carousel's
 * neighbor falloff); wheel walks albums row-major and pages at the edges.
 */
#include "ui_internal.h"
#include "thumb.h"

#define TILE_PX   116
#define TILE_X0   38
#define TILE_Y0   70
#define TILE_PITCH 136
#define PER_PAGE  8

static size_t    grid_sel;
static int       grid_page = -1;
static lv_obj_t *tiles[PER_PAGE];
static lv_obj_t *name_lbl, *artist_lbl;
static lv_obj_t *grid_scr;

static void paint_label(void)
{
    const album_t *al = &ui_lib->albums[grid_sel];
    lv_label_set_text(name_lbl, al->album);
    lv_label_set_text(artist_lbl, al->artist);
}

static void paint_tiles(void)
{
    int page = (int)(grid_sel / PER_PAGE);
    if (page != grid_page) {
        grid_page = page;
        for (int i = 0; i < PER_PAGE; i++) {
            lv_obj_t *t = tiles[i];
            lv_obj_clean(t);
            size_t alb = (size_t)page * PER_PAGE + i;
            if (alb >= ui_lib->album_count) {
                lv_obj_add_flag(t, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(t, LV_OBJ_FLAG_HIDDEN);
            const char *art =
                ui_art_provider ? ui_art_provider(alb, TILE_PX) : NULL;
            if (art) {
                lv_obj_t *img = lv_image_create(t);
                lv_image_set_src(img, art);
                lv_obj_set_pos(img, 0, 0);
            }
        }
    }
    for (int i = 0; i < PER_PAGE; i++)
        lv_obj_set_style_opa(tiles[i],
            (size_t)(grid_page * PER_PAGE + i) == grid_sel
                ? LV_OPA_COVER : PACT_OPA_NEAR, 0);
    paint_label();
}

void ui_show_grid(void)
{
    ui_cur_screen = UI_SCR_GRID;
    grid_page = -1;
    if (grid_sel >= ui_lib->album_count) grid_sel = 0;
    grid_scr = ui_screen_new();

    lv_obj_t *title = lv_label_create(grid_scr);
    lv_label_set_text(title, "Albums");
    lv_obj_set_style_text_font(title, &diatype_medium_18, 0);
    lv_obj_set_style_text_color(title, PACT_COL_TEXT, 0);
    lv_obj_set_pos(title, 38, 26);
    ui_battery_create(grid_scr);

    for (int i = 0; i < PER_PAGE; i++) {
        lv_obj_t *t = lv_obj_create(grid_scr);
        lv_obj_set_size(t, TILE_PX, TILE_PX);
        lv_obj_set_pos(t, TILE_X0 + (i % 4) * TILE_PITCH,
                       TILE_Y0 + (i / 4) * TILE_PITCH);
        lv_obj_set_style_bg_color(t, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(t, 4, 0);
        lv_obj_set_style_clip_corner(t, true, 0);
        lv_obj_set_style_border_width(t, 0, 0);
        lv_obj_set_style_pad_all(t, 0, 0);
        lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(t, LV_SCROLLBAR_MODE_OFF);
        tiles[i] = t;
    }

    name_lbl = lv_label_create(grid_scr);
    lv_obj_set_style_text_font(name_lbl, &diatype_medium_18, 0);
    lv_obj_set_style_text_color(name_lbl, PACT_COL_TEXT, 0);
    lv_obj_set_pos(name_lbl, 38, 344);
    lv_obj_set_width(name_lbl, 420);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);

    artist_lbl = lv_label_create(grid_scr);
    lv_obj_set_style_text_font(artist_lbl, &diatype_regular_15, 0);
    lv_obj_set_style_text_color(artist_lbl, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_pos(artist_lbl, 38, 370);


    /* invisible key sink: routes wheel/buttons to this screen's handler */
    lv_obj_t *sink = lv_obj_create(grid_scr);
    lv_obj_set_size(sink, 1, 1);
    lv_obj_set_style_bg_opa(sink, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sink, 0, 0);
    ui_bind_keys(sink);

    paint_tiles();
    ui_screen_show(grid_scr);
}

void ui_grid_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (grid_sel + 1 < ui_lib->album_count) { grid_sel++; paint_tiles(); }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (grid_sel > 0) { grid_sel--; paint_tiles(); }
        break;
    case PACT_EVT_CENTER:
        ui_tracks_back = UI_SCR_GRID;
        ui_show_tracks(grid_sel);
        break;
    case PACT_EVT_UP:
        ui_show_menu();
        break;
    default: break;
    }
}
