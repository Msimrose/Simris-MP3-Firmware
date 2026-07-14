/*
 * Pact MP-1 - Now Playing, NP-A "Centered", exact to Figma node 10:2.
 * One deliberate addition over the mock: the format badge (FLAC · 24/48)
 * centered between the time stamps, per the agreed variant-A brief.
 */
#include "ui_internal.h"
#include "../audio/audio_engine.h"
#include <stdio.h>
#include <string.h>

#define COVER    232
#define BAR_W    300
#define BAR_X    150   /* (600 - BAR_W) / 2 */
#define BAR_Y    384

static lv_obj_t *np_title, *np_artist, *np_badge, *np_state;
static lv_obj_t *np_bar_fill, *np_elapsed, *np_total;

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
    fmt_time(buf, sizeof(buf), pos_ms);
    lv_label_set_text(np_elapsed, buf);
    fmt_time(buf, sizeof(buf), tr->t.duration_ms);
    lv_label_set_text(np_total, buf);
    lv_obj_update_layout(np_total);
    lv_obj_set_x(np_total, BAR_X + BAR_W - lv_obj_get_width(np_total));

    int32_t w = tr->t.duration_ms
                    ? (int32_t)((uint64_t)pos_ms * BAR_W / tr->t.duration_ms)
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
