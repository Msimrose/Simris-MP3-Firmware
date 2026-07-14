/*
 * Pact MP-1 - album carousel (Figma 08 "flat rail").
 *
 * Center cover large and full-bright, neighbors smaller and dimmed, no
 * overlap. Motion ported from the threejs-coverflow study: long ease-out
 * glide on position with a faster clock on scale/opacity, and animations
 * retarget mid-flight so wheel spam feels liquid instead of queued.
 */
#include "ui_internal.h"
#include "thumb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAIL_CY        190   /* cover center line */
#define RAIL_SPACING   130   /* slot pitch */
#define COVER_PX       190   /* source thumb size = center size */
#define POS_MS         420   /* position glide */
#define XFORM_MS       220   /* scale/opacity clock (about half) */

static lv_obj_t       **covers;
static lv_draw_buf_t  **cover_bufs;   /* covers decoded to RAM: transforms
                                       * only work on raw-buffer sources */
static size_t           count;
static int               sel;
static lv_obj_t         *lbl_title, *lbl_artist, *lbl_counter;
static lv_obj_t        **dots;
static size_t            dots_n;
#define MAX_DOTS 15

/* Art provider hands back "A:<path>.jpg"; the pre-scaled BMP twin of that
 * file is what we can actually load into RAM for scale transforms. */
static lv_draw_buf_t *load_cover_ram(const char *art)
{
    if (!art) return NULL;
    const char *path = (art[0] && art[1] == ':') ? art + 2 : art;
    char bmp[512];
    size_t n = strlen(path);
    if (n < 4 || n >= sizeof(bmp)) return NULL;
    memcpy(bmp, path, n + 1);
    memcpy(bmp + n - 4, ".bmp", 4);
    return pact_thumb_load_bmp(bmp);
}

/* scale (LVGL 256 = 1.0) and opacity by distance (Figma: 55% / 30%) */
static const int16_t scale_by_d[] = { 256, 202, 135, 135 };
static const uint8_t opa_by_d[]   = { 255, 140,  77,   0 };

static int16_t slot_scale(int d) { d = abs(d); return scale_by_d[d > 3 ? 3 : d]; }
static uint8_t slot_opa(int d)   { d = abs(d); return opa_by_d[d > 3 ? 3 : d]; }
static int32_t slot_x(int d)     { return 300 + d * RAIL_SPACING - COVER_PX / 2; }

static void anim_x_cb(void *var, int32_t v)     { lv_obj_set_x(var, v); }
static void anim_scale_cb(void *var, int32_t v) { lv_image_set_scale(var, (uint32_t)v); }
static void anim_opa_cb(void *var, int32_t v)   { lv_obj_set_style_opa(var, (lv_opa_t)v, 0); }

static void animate_to(lv_obj_t *obj, int32_t x, int32_t scale, int32_t opa,
                       bool instant)
{
    if (instant) {
        lv_obj_set_x(obj, x);
        lv_image_set_scale(obj, (uint32_t)scale);
        lv_obj_set_style_opa(obj, (lv_opa_t)opa, 0);
        return;
    }
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_values(&a, lv_obj_get_x(obj), x);
    lv_anim_set_duration(&a, POS_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_scale_cb);
    lv_anim_set_values(&a, (int32_t)lv_image_get_scale(obj), scale);
    lv_anim_set_duration(&a, XFORM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, lv_obj_get_style_opa(obj, 0), opa);
    lv_anim_set_duration(&a, XFORM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void carousel_layout(bool instant)
{
    for (size_t i = 0; i < count; i++) {
        if (!covers[i]) continue;
        int d = (int)i - sel;
        animate_to(covers[i], slot_x(d), slot_scale(d), slot_opa(d), instant);
    }
    /* stacking: nearer covers over farther ones, center on top */
    for (int dist = 3; dist >= 0; dist--) {
        for (size_t i = 0; i < count; i++) {
            if (!covers[i]) continue;
            if (abs((int)i - sel) == dist) lv_obj_move_foreground(covers[i]);
        }
    }
    lv_obj_move_foreground(lbl_title);
    lv_obj_move_foreground(lbl_artist);
    lv_obj_move_foreground(lbl_counter);
    for (size_t i = 0; i < dots_n; i++) lv_obj_move_foreground(dots[i]);
    const album_t *al = &ui_lib->albums[sel];
    lv_label_set_text(lbl_title, al->album[0] ? al->album : "(unknown album)");
    lv_label_set_text(lbl_artist, al->artist[0] ? al->artist : "(unknown)");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 312);
    lv_obj_align(lbl_artist, LV_ALIGN_TOP_MID, 0, 343);
    if (dots_n) {
        lv_label_set_text(lbl_counter, "");
        for (size_t i = 0; i < dots_n; i++)
            lv_obj_set_style_bg_opa(dots[i],
                                    (int)i == sel ? LV_OPA_COVER : PACT_OPA_DOT, 0);
    } else {
        lv_label_set_text_fmt(lbl_counter, "%d / %zu", sel + 1, count);
        lv_obj_align(lbl_counter, LV_ALIGN_TOP_MID, 0, 376);
    }
}

void ui_carousel_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
    case PACT_EVT_RIGHT:
        if (sel < (int)count - 1) { sel++; carousel_layout(false); }
        break;
    case PACT_EVT_WHEEL_CCW:
    case PACT_EVT_LEFT:
        if (sel > 0) { sel--; carousel_layout(false); }
        break;
    case PACT_EVT_CENTER:
        ui_tracks_back = UI_SCR_CAROUSEL;
        ui_show_tracks((size_t)sel);
        break;
    case PACT_EVT_UP:
        ui_show_menu();
        break;
    default: break;
    }
}

void ui_show_carousel(void)
{
    ui_cur_screen = UI_SCR_CAROUSEL;
    count = ui_lib->album_count;

    free(covers);
    covers = calloc(count, sizeof(lv_obj_t *));

    /* the previous visit's decoded covers must outlive its screen: stash
     * them and free only after ui_screen_show() has deleted that screen */
    static size_t bufs_alloc;
    lv_draw_buf_t **old_bufs = cover_bufs;
    size_t old_alloc = bufs_alloc;
    cover_bufs = calloc(count, sizeof(lv_draw_buf_t *));
    bufs_alloc = count;

    lv_obj_t *scr = ui_screen_new();

    for (size_t i = 0; i < count; i++) {
        const char *art = ui_art_provider ? ui_art_provider(i, COVER_PX) : NULL;
        if (art) cover_bufs[i] = load_cover_ram(art);
        lv_obj_t *cov;
        if (cover_bufs[i]) {
            cov = lv_image_create(scr);
            lv_image_set_src(cov, cover_bufs[i]);
        } else {
            cov = lv_obj_create(scr);
            lv_obj_set_style_bg_color(cov, PACT_COL_SELECT, 0);
            lv_obj_set_style_bg_opa(cov, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(cov, 0, 0);
        }
        lv_obj_set_size(cov, COVER_PX, COVER_PX);
        lv_obj_set_y(cov, RAIL_CY - COVER_PX / 2);
        if (cover_bufs[i]) {
            /* pivot 119 (not center): reproduces Figma's stepped tops,
             * where smaller neighbors sit slightly lower (y95/120/150) */
            lv_obj_set_style_transform_pivot_x(cov, COVER_PX / 2, 0);
            lv_obj_set_style_transform_pivot_y(cov, 119, 0);
        }
        lv_obj_set_style_radius(cov, 5, 0);
        lv_obj_set_style_clip_corner(cov, true, 0);
        lv_obj_clear_flag(cov, LV_OBJ_FLAG_SCROLLABLE);
        covers[i] = cov;
    }

    lbl_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_title, &diatype_medium_22, 0);
    lv_obj_set_style_text_color(lbl_title, PACT_COL_TEXT, 0);

    lbl_artist = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_artist, &diatype_regular_14, 0);
    lv_obj_set_style_text_color(lbl_artist, PACT_COL_TEXT_DIM, 0);

    lbl_counter = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_counter, &diatype_regular_14, 0);
    lv_obj_set_style_text_color(lbl_counter, PACT_COL_TEXT_DIM, 0);

    /* Figma dot indicator (6px squares, r1, 14px pitch) for small shelves;
     * large libraries get the counter instead */
    free(dots);
    dots = NULL;
    dots_n = 0;
    if (count <= MAX_DOTS) {
        dots_n = count;
        dots = calloc(dots_n, sizeof(lv_obj_t *));
        int32_t x0 = 300 - ((int32_t)dots_n * 14 - 8) / 2;
        for (size_t i = 0; i < dots_n; i++) {
            lv_obj_t *d = lv_obj_create(scr);
            lv_obj_set_size(d, 6, 6);
            lv_obj_set_pos(d, x0 + (int32_t)i * 14, 378);
            lv_obj_set_style_bg_color(d, PACT_COL_TEXT, 0);
            lv_obj_set_style_border_width(d, 0, 0);
            lv_obj_set_style_radius(d, 1, 0);
            lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
            dots[i] = d;
        }
    }

    /* land on the playing album if there is one */
    if (ui_current_track != UI_NO_TRACK) {
        size_t alb = ui_album_of_track(ui_current_track);
        if (alb != (size_t)-1) sel = (int)alb;
    }
    if (sel >= (int)count) sel = 0;

    carousel_layout(true);

    lv_obj_t *sink = lv_obj_create(scr);
    lv_obj_set_size(sink, 1, 1);
    lv_obj_set_style_bg_opa(sink, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sink, 0, 0);
    ui_bind_keys(sink);

    ui_screen_show(scr);

    if (old_bufs) {
        for (size_t i = 0; i < old_alloc; i++)
            if (old_bufs[i]) lv_draw_buf_destroy(old_bufs[i]);
        free(old_bufs);
    }
}
