/*
 * Pact MP-1 - Now Playing.
 *   NP-A "Centered"  (Figma 10:2)  - default; format badge is the one
 *                                    agreed addition over the mock.
 *   NP-B "Immersive" (Figma 17:66) - full-bleed cover (232 thumb RAM-
 *                                    scaled ~2.6x), bottom scrim gradient,
 *                                    title/artist bottom-left, full-width
 *                                    hairline progress, no times/badge.
 * Chosen in Settings -> Now Playing (pact_settings.np_view).
 */
#include "ui_internal.h"
#include "thumb.h"
#include "../audio/audio_engine.h"
#include "../settings/pact_settings.h"
#include <stdio.h>
#include <string.h>

#define COVER    232
#define BAR_W    300
#define BAR_X    150   /* (600 - BAR_W) / 2 */
#define BAR_Y    384

static lv_obj_t *np_title, *np_artist, *np_badge, *np_state;
static lv_obj_t *np_bar_fill, *np_elapsed, *np_total;
static int32_t   np_bar_w = BAR_W;      /* variant-dependent */

/* immersive backdrop RAM buffers: free one build late so the previous
 * screen's async delete never renders a dead buffer */
static lv_draw_buf_t *np_bg_cur, *np_bg_old;

static void fmt_time(char *out, size_t cap, uint32_t ms)
{
    snprintf(out, cap, "%u:%02u", ms / 60000, (ms / 1000) % 60);
}

static const char *ext_upper(const char *path)
{
    const char *dot = path ? strrchr(path, '.') : NULL;
    if (!dot) return "";
    static char up[8];
    size_t i = 0;
    for (dot++; *dot && i < sizeof(up) - 1; dot++, i++)
        up[i] = (char)((*dot >= 'a' && *dot <= 'z') ? *dot - 32 : *dot);
    up[i] = '\0';
    return up;
}

void ui_nowplaying_refresh(void)
{
    if (ui_cur_screen != UI_SCR_NOWPLAYING || ui_current_track == UI_NO_TRACK)
        return;
    if (!np_bar_fill || !lv_obj_is_valid(np_bar_fill)) return;
    const track_t *tr = &ui_lib->tracks[ui_current_track];

    uint32_t pos_ms = (uint32_t)(audio_engine_position() * 1000.0);
    if (tr->t.duration_ms && pos_ms > tr->t.duration_ms)
        pos_ms = tr->t.duration_ms;

    char buf[16];
    if (np_elapsed) {
        fmt_time(buf, sizeof(buf), pos_ms);
        lv_label_set_text(np_elapsed, buf);
    }
    if (np_total) {
        fmt_time(buf, sizeof(buf), tr->t.duration_ms);
        lv_label_set_text(np_total, buf);
        lv_obj_update_layout(np_total);
        lv_obj_set_x(np_total, BAR_X + BAR_W - lv_obj_get_width(np_total));
    }

    int32_t w = tr->t.duration_ms
                    ? (int32_t)((uint64_t)pos_ms * np_bar_w / tr->t.duration_ms)
                    : 0;
    lv_obj_set_width(np_bar_fill, w > 3 ? w : 3);

    lv_label_set_text(np_state,
                      audio_engine_state() == ENGINE_PAUSED ? "paused" : "");
}

void ui_show_nowplaying(void)
{
    if (ui_current_track == UI_NO_TRACK) return;
    ui_cur_screen = UI_SCR_NOWPLAYING;
    const track_t *tr = &ui_lib->tracks[ui_current_track];
    size_t alb = ui_album_of_track(ui_current_track);

    lv_obj_t *scr = ui_screen_new();

    if (pact_settings.np_view) {        /* ---- NP-B Immersive ---- */
        np_bar_w = 524;
        np_elapsed = np_total = np_badge = NULL;

        if (np_bg_old) { lv_draw_buf_destroy(np_bg_old); np_bg_old = NULL; }
        np_bg_old = np_bg_cur;
        np_bg_cur = NULL;

        const char *art = (ui_art_provider && alb != (size_t)-1)
                              ? ui_art_provider(alb, 232) : NULL;
        if (art) {
            /* BMP twin -> RAM (same trick as the carousel: file-sourced
             * JPEGs cannot scale-transform in LVGL 9.4) */
            const char *path = (art[0] && art[1] == ':') ? art + 2 : art;
            char bmp[512];
            size_t n = strlen(path);
            if (n > 4 && n < sizeof bmp) {
                memcpy(bmp, path, n + 1);
                memcpy(bmp + n - 4, ".bmp", 4);
                np_bg_cur = pact_thumb_load_bmp(bmp);
            }
        }
        if (np_bg_cur) {
            lv_obj_t *bg = lv_image_create(scr);
            lv_image_set_src(bg, np_bg_cur);
            lv_image_set_pivot(bg, 0, 0);
            lv_image_set_scale(bg, 600 * 256 / 232);   /* fill width */
            lv_obj_set_pos(bg, 0, -75);                /* center 600px crop */
        }

        /* scrim: gentle overall dark + bottom gradient for text */
        lv_obj_t *dark = lv_obj_create(scr);
        lv_obj_set_size(dark, 600, 450);
        lv_obj_set_pos(dark, 0, 0);
        lv_obj_set_style_bg_color(dark, PACT_COL_GROUND, 0);
        lv_obj_set_style_bg_opa(dark, 64, 0);
        lv_obj_set_style_border_width(dark, 0, 0);
        lv_obj_set_style_radius(dark, 0, 0);

        static lv_grad_dsc_t grad;                     /* style keeps the ptr */
        grad.dir = LV_GRAD_DIR_VER;
        grad.stops_count = 2;
        grad.stops[0].color = lv_color_black();
        grad.stops[0].opa   = LV_OPA_0;
        grad.stops[0].frac  = 0;
        grad.stops[1].color = lv_color_black();
        grad.stops[1].opa   = 220;
        grad.stops[1].frac  = 255;
        lv_obj_t *scrim = lv_obj_create(scr);
        lv_obj_set_size(scrim, 600, 190);
        lv_obj_set_pos(scrim, 0, 260);
        lv_obj_set_style_bg_grad(scrim, &grad, 0);
        lv_obj_set_style_bg_opa(scrim, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(scrim, 0, 0);
        lv_obj_set_style_radius(scrim, 0, 0);

        ui_battery_create_at(scr, 22);

        np_title = lv_label_create(scr);
        lv_label_set_text(np_title, tr->t.title);
        lv_obj_set_style_text_font(np_title, &diatype_medium_27, 0);
        lv_obj_set_style_text_color(np_title, PACT_COL_TEXT, 0);
        lv_label_set_long_mode(np_title, LV_LABEL_LONG_DOT);
        lv_obj_set_width(np_title, 524);
        lv_obj_set_pos(np_title, 38, 316);

        np_artist = lv_label_create(scr);
        lv_label_set_text(np_artist, tr->t.artist);
        lv_obj_set_style_text_font(np_artist, &diatype_regular_15, 0);
        lv_obj_set_style_text_color(np_artist, PACT_COL_TEXT_DIM, 0);
        lv_label_set_long_mode(np_artist, LV_LABEL_LONG_DOT);
        lv_obj_set_width(np_artist, 400);
        lv_obj_set_pos(np_artist, 38, 356);

        np_state = lv_label_create(scr);
        lv_obj_set_style_text_font(np_state, &diatype_regular_11, 0);
        lv_obj_set_style_text_color(np_state, PACT_COL_TEXT_DIM, 0);
        lv_label_set_text(np_state, "");
        lv_obj_set_pos(np_state, 38, 388);

        lv_obj_t *btrack = lv_obj_create(scr);
        lv_obj_set_size(btrack, np_bar_w, 2);
        lv_obj_set_pos(btrack, 38, 412);
        lv_obj_set_style_bg_color(btrack, PACT_COL_TEXT, 0);
        lv_obj_set_style_bg_opa(btrack, PACT_OPA_TRACK, 0);
        lv_obj_set_style_border_width(btrack, 0, 0);
        lv_obj_set_style_radius(btrack, 1, 0);

        np_bar_fill = lv_obj_create(scr);
        lv_obj_set_size(np_bar_fill, 3, 2);
        lv_obj_set_pos(np_bar_fill, 38, 412);
        lv_obj_set_style_bg_color(np_bar_fill, PACT_COL_TEXT, 0);
        lv_obj_set_style_bg_opa(np_bar_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(np_bar_fill, 0, 0);
        lv_obj_set_style_radius(np_bar_fill, 1, 0);

        lv_obj_t *sink2 = lv_obj_create(scr);
        lv_obj_set_size(sink2, 1, 1);
        lv_obj_set_style_bg_opa(sink2, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(sink2, 0, 0);

        ui_nowplaying_refresh();
        ui_bind_keys(sink2);
        ui_screen_show(scr);
        return;
    }

    np_bar_w = BAR_W;                   /* ---- NP-A Centered ---- */

    /* brand wordmark (Figma 9:2 placement) + battery */
    lv_obj_t *logo = lv_image_create(scr);
    lv_image_set_src(logo, &simris_logo);
    lv_obj_set_pos(logo, 18, 17);

    ui_battery_create_at(scr, 22);

    /* centered cover, 232px, r6 */
    const char *art =
        (ui_art_provider && alb != (size_t)-1) ? ui_art_provider(alb, COVER) : NULL;
    if (art) {
        lv_obj_t *cov = lv_image_create(scr);
        lv_image_set_src(cov, art);
        lv_obj_set_size(cov, COVER, COVER);
        lv_obj_set_pos(cov, (600 - COVER) / 2, 42);
        lv_obj_set_style_radius(cov, 6, 0);
        lv_obj_set_style_clip_corner(cov, true, 0);
    } else {
        lv_obj_t *ph = lv_obj_create(scr);
        lv_obj_set_size(ph, COVER, COVER);
        lv_obj_set_pos(ph, (600 - COVER) / 2, 42);
        lv_obj_set_style_bg_color(ph, PACT_COL_SELECT, 0);
        lv_obj_set_style_border_width(ph, 0, 0);
        lv_obj_set_style_radius(ph, 6, 0);
    }

    /* title: Medium 27, warm white, centered, y298 */
    np_title = lv_label_create(scr);
    lv_label_set_text(np_title, tr->t.title);
    lv_obj_set_style_text_font(np_title, &diatype_medium_27, 0);
    lv_obj_set_style_text_color(np_title, PACT_COL_TEXT, 0);
    lv_label_set_long_mode(np_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(np_title, 520);
    lv_obj_set_style_text_align(np_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(np_title, LV_ALIGN_TOP_MID, 0, 298);

    /* artist: Regular 15, dim, centered, y336 */
    np_artist = lv_label_create(scr);
    lv_label_set_text(np_artist, tr->t.artist);
    lv_obj_set_style_text_font(np_artist, &diatype_regular_15, 0);
    lv_obj_set_style_text_color(np_artist, PACT_COL_TEXT_DIM, 0);
    lv_label_set_long_mode(np_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(np_artist, 400);
    lv_obj_set_style_text_align(np_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(np_artist, LV_ALIGN_TOP_MID, 0, 336);

    /* paused state (addition): Regular 11, dim, centered above the bar */
    np_state = lv_label_create(scr);
    lv_obj_set_style_text_font(np_state, &diatype_regular_11, 0);
    lv_obj_set_style_text_color(np_state, PACT_COL_TEXT_DIM, 0);
    lv_label_set_text(np_state, "");
    lv_obj_align(np_state, LV_ALIGN_TOP_MID, 0, 364);

    /* progress: 300x3 track (warm white 16%) + warm-white fill, r2, y384 */
    lv_obj_t *track = lv_obj_create(scr);
    lv_obj_set_size(track, BAR_W, 3);
    lv_obj_set_pos(track, BAR_X, BAR_Y);
    lv_obj_set_style_bg_color(track, PACT_COL_TEXT, 0);
    lv_obj_set_style_bg_opa(track, PACT_OPA_TRACK, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_radius(track, 2, 0);

    np_bar_fill = lv_obj_create(scr);
    lv_obj_set_size(np_bar_fill, 3, 3);
    lv_obj_set_pos(np_bar_fill, BAR_X, BAR_Y);
    lv_obj_set_style_bg_color(np_bar_fill, PACT_COL_TEXT, 0);
    lv_obj_set_style_bg_opa(np_bar_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(np_bar_fill, 0, 0);
    lv_obj_set_style_radius(np_bar_fill, 2, 0);

    /* times: Regular 11, dim, y394 */
    np_elapsed = lv_label_create(scr);
    lv_obj_set_style_text_font(np_elapsed, &diatype_regular_11, 0);
    lv_obj_set_style_text_color(np_elapsed, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_pos(np_elapsed, BAR_X, BAR_Y + 10);

    np_total = lv_label_create(scr);
    lv_obj_set_style_text_font(np_total, &diatype_regular_11, 0);
    lv_obj_set_style_text_color(np_total, PACT_COL_TEXT_DIM, 0);
    lv_label_set_text(np_total, "0:00");
    lv_obj_set_pos(np_total, BAR_X + BAR_W - 22, BAR_Y + 10);

    /* format badge (addition): Regular 11, dim, centered on the times row */
    np_badge = lv_label_create(scr);
    if (tr->t.bits_per_sample)
        lv_label_set_text_fmt(np_badge, "%s · %u/%u", ext_upper(tr->path),
                              tr->t.bits_per_sample, tr->t.sample_rate / 1000);
    else
        lv_label_set_text_fmt(np_badge, "%s · %u kHz", ext_upper(tr->path),
                              tr->t.sample_rate / 1000);
    lv_obj_set_style_text_font(np_badge, &diatype_regular_11, 0);
    lv_obj_set_style_text_color(np_badge, PACT_COL_TEXT_DIM, 0);
    lv_obj_align(np_badge, LV_ALIGN_TOP_MID, 0, BAR_Y + 10);

    lv_obj_t *sink = lv_obj_create(scr);   /* invisible key receiver */
    lv_obj_set_size(sink, 1, 1);
    lv_obj_set_style_bg_opa(sink, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sink, 0, 0);

    ui_nowplaying_refresh();
    ui_bind_keys(sink);
    ui_screen_show(scr);
}

void ui_nowplaying_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:   /* wheel on Now Playing = volume (iPod-style) */
        ui_volume_step(+1);
        break;
    case PACT_EVT_WHEEL_CCW:
        ui_volume_step(-1);
        break;
    case PACT_EVT_CENTER:
        if (audio_engine_state() == ENGINE_PLAYING) audio_engine_pause();
        else if (audio_engine_state() == ENGINE_PAUSED) audio_engine_resume();
        ui_nowplaying_refresh();
        break;
    case PACT_EVT_LEFT:   /* >3s in: restart track; else previous */
        if (ui_current_track != UI_NO_TRACK) {
            if (audio_engine_position() > 3.0) {
                audio_engine_seek(0.0);
                ui_nowplaying_refresh();
                break;
            }
            size_t alb = ui_album_of_track(ui_current_track);
            if (alb != (size_t)-1 && ui_current_track > ui_lib->albums[alb].first)
                ui_play_track(ui_current_track - 1);
        }
        break;
    case PACT_EVT_RIGHT:  /* next track in album */
        if (ui_current_track != UI_NO_TRACK) {
            size_t alb = ui_album_of_track(ui_current_track);
            if (alb != (size_t)-1 &&
                ui_current_track + 1 <
                    ui_lib->albums[alb].first + ui_lib->albums[alb].count)
                ui_play_track(ui_current_track + 1);
        }
        break;
    case PACT_EVT_UP: {   /* back to the album's track list */
        size_t alb = ui_album_of_track(ui_current_track);
        if (alb != (size_t)-1) ui_show_tracks(alb);
        else ui_show_menu();
        break;
    }
    default: break;
    }
}
