/*
 * Pact MP-1 - Now Playing, variant NP-A "Centered" (Figma 10:2).
 * Art as the centered jewel; brand mark top-left, battery top-right,
 * one title line, quiet artist line, thin full-width progress with the
 * format badge restored between the time stamps.
 */
#include "ui_internal.h"
#include "../audio/audio_engine.h"
#include <stdio.h>
#include <string.h>

#define COVER    232
#define BAR_W    300
#define BAR_X    150
#define BAR_Y    384

static lv_obj_t *np_title, *np_artist, *np_badge, *np_state;
static lv_obj_t *np_bar, *np_elapsed, *np_total;

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
    if (!np_bar || !lv_obj_is_valid(np_bar)) return;
    const track_t *tr = &ui_lib->tracks[ui_current_track];

    uint32_t pos_ms = (uint32_t)(audio_engine_position() * 1000.0);
    if (tr->t.duration_ms && pos_ms > tr->t.duration_ms)
        pos_ms = tr->t.duration_ms;

    char buf[16];
    fmt_time(buf, sizeof(buf), pos_ms);
    lv_label_set_text(np_elapsed, buf);
    fmt_time(buf, sizeof(buf), tr->t.duration_ms);
    lv_label_set_text(np_total, buf);
    lv_obj_align(np_total, LV_ALIGN_TOP_LEFT, BAR_X + BAR_W, BAR_Y + 10);
    lv_obj_set_x(np_total, BAR_X + BAR_W - lv_obj_get_width(np_total));

    int32_t pct = tr->t.duration_ms
                      ? (int32_t)((uint64_t)pos_ms * 100 / tr->t.duration_ms)
                      : 0;
    lv_bar_set_value(np_bar, pct, LV_ANIM_OFF);

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

    ui_brand_mark(scr, 18, 17);
    ui_battery_create(scr);

    /* centered cover */
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

    np_title = lv_label_create(scr);
    lv_label_set_text(np_title, tr->t.title);
    lv_obj_set_style_text_font(np_title, &diatype_medium_32, 0);
    lv_obj_set_style_text_color(np_title, PACT_COL_TEXT, 0);
    lv_label_set_long_mode(np_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(np_title, 520);
    lv_obj_set_style_text_align(np_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(np_title, LV_ALIGN_TOP_MID, 0, 296);

    np_artist = lv_label_create(scr);
    lv_label_set_text(np_artist, tr->t.artist);
    lv_obj_set_style_text_font(np_artist, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(np_artist, PACT_COL_TEXT_DIM, 0);
    lv_label_set_long_mode(np_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(np_artist, 400);
    lv_obj_set_style_text_align(np_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(np_artist, LV_ALIGN_TOP_MID, 0, 338);

    /* progress bar + times + badge */
    np_bar = lv_bar_create(scr);
    lv_obj_set_size(np_bar, BAR_W, 3);
    lv_obj_set_pos(np_bar, BAR_X, BAR_Y);
    lv_bar_set_range(np_bar, 0, 100);
    lv_obj_set_style_bg_color(np_bar, PACT_COL_SELECT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(np_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(np_bar, PACT_COL_WHITE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(np_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(np_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(np_bar, 2, LV_PART_INDICATOR);

    np_elapsed = lv_label_create(scr);
    lv_obj_set_style_text_font(np_elapsed, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(np_elapsed, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_pos(np_elapsed, BAR_X, BAR_Y + 10);

    np_total = lv_label_create(scr);
    lv_obj_set_style_text_font(np_total, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(np_total, PACT_COL_TEXT_DIM, 0);
    lv_label_set_text(np_total, "0:00");
    lv_obj_set_pos(np_total, BAR_X + BAR_W - 40, BAR_Y + 10);

    np_badge = lv_label_create(scr);
    if (tr->t.bits_per_sample)
        lv_label_set_text_fmt(np_badge, "%s · %u/%u", ext_upper(tr->path),
                              tr->t.bits_per_sample, tr->t.sample_rate / 1000);
    else
        lv_label_set_text_fmt(np_badge, "%s · %u kHz", ext_upper(tr->path),
                              tr->t.sample_rate / 1000);
    lv_obj_set_style_text_font(np_badge, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(np_badge, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_text_align(np_badge, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(np_badge, LV_ALIGN_TOP_MID, 0, BAR_Y + 10);

    np_state = lv_label_create(scr);
    lv_obj_set_style_text_font(np_state, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(np_state, PACT_COL_TEXT_DIM, 0);
    lv_label_set_text(np_state, "");
    lv_obj_align(np_state, LV_ALIGN_TOP_MID, 0, 366);

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
