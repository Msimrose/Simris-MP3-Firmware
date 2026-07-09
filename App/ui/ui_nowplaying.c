/*
 * Pact MP-1 - Now Playing: cover left, info right (landscape split per
 * brand-ui-system.md). Placeholder spacing until the Figma design lands.
 */
#include "ui_internal.h"
#include "../audio/audio_engine.h"
#include <stdio.h>
#include <string.h>

#define PAD     40
#define COVER   280

static lv_obj_t *np_scr;
static lv_obj_t *np_title, *np_artist, *np_album, *np_badge, *np_state;
static lv_obj_t *np_bar, *np_elapsed, *np_total, *np_vol;
static size_t    np_built_for = UI_NO_TRACK;

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
    const track_t *tr = &ui_lib->tracks[ui_current_track];

    uint32_t pos_ms = (uint32_t)(audio_engine_position() * 1000.0);
    if (tr->t.duration_ms && pos_ms > tr->t.duration_ms)
        pos_ms = tr->t.duration_ms;

    char buf[16];
    fmt_time(buf, sizeof(buf), pos_ms);
    lv_label_set_text(np_elapsed, buf);
    fmt_time(buf, sizeof(buf), tr->t.duration_ms);
    lv_label_set_text(np_total, buf);

    int32_t pct = tr->t.duration_ms
                      ? (int32_t)((uint64_t)pos_ms * 100 / tr->t.duration_ms)
                      : 0;
    lv_bar_set_value(np_bar, pct, LV_ANIM_OFF);

    engine_state_t st = audio_engine_state();
    lv_label_set_text(np_state, st == ENGINE_PAUSED ? "paused" : "");

    lv_label_set_text_fmt(np_vol, "vol %d", audio_engine_volume());
}

void ui_show_nowplaying(void)
{
    if (ui_current_track == UI_NO_TRACK) return;
    ui_cur_screen = UI_SCR_NOWPLAYING;
    const track_t *tr = &ui_lib->tracks[ui_current_track];
    size_t alb = ui_album_of_track(ui_current_track);

    lv_obj_t *scr = lv_obj_create(NULL);
    np_scr = scr;
    np_built_for = ui_current_track;
    lv_obj_set_style_bg_color(scr, PACT_COL_GROUND, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* cover, left half */
    const char *art =
        (ui_art_provider && alb != (size_t)-1) ? ui_art_provider(alb, COVER) : NULL;
    if (art) {
        lv_obj_t *cov = lv_image_create(scr);
        lv_image_set_src(cov, art);
        lv_obj_set_size(cov, COVER, COVER);
        lv_obj_align(cov, LV_ALIGN_LEFT_MID, PAD, 0);
        lv_obj_set_style_radius(cov, 6, 0);
        lv_obj_set_style_clip_corner(cov, true, 0);
    } else {
        lv_obj_t *ph = lv_obj_create(scr);
        lv_obj_set_size(ph, COVER, COVER);
        lv_obj_align(ph, LV_ALIGN_LEFT_MID, PAD, 0);
        lv_obj_set_style_bg_color(ph, PACT_COL_SELECT, 0);
        lv_obj_set_style_border_width(ph, 0, 0);
        lv_obj_set_style_radius(ph, 6, 0);
    }

    /* info column, right half */
    int32_t rx = PAD + COVER + 28;
    int32_t rw = 600 - rx - PAD;

    np_title = lv_label_create(scr);
    lv_label_set_text(np_title, tr->t.title);
    lv_obj_set_style_text_font(np_title, &diatype_medium_32, 0);
    lv_obj_set_style_text_color(np_title, PACT_COL_TEXT, 0);
    lv_label_set_long_mode(np_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(np_title, rw);
    lv_obj_set_pos(np_title, rx, 110);

    np_artist = lv_label_create(scr);
    lv_label_set_text(np_artist, tr->t.artist);
    lv_obj_set_style_text_font(np_artist, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(np_artist, PACT_COL_TEXT_DIM, 0);
    lv_label_set_long_mode(np_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(np_artist, rw);
    lv_obj_set_pos(np_artist, rx, 190);

    np_album = lv_label_create(scr);
    lv_label_set_text(np_album, tr->t.album);
    lv_obj_set_style_text_font(np_album, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(np_album, PACT_COL_TEXT_DIM, 0);
    lv_label_set_long_mode(np_album, LV_LABEL_LONG_DOT);
    lv_obj_set_width(np_album, rw);
    lv_obj_set_pos(np_album, rx, 214);

    /* format badge, e.g. "FLAC 24/48" */
    np_badge = lv_label_create(scr);
    if (tr->t.bits_per_sample)
        lv_label_set_text_fmt(np_badge, "%s %u/%u", ext_upper(tr->path),
                              tr->t.bits_per_sample, tr->t.sample_rate / 1000);
    else
        lv_label_set_text_fmt(np_badge, "%s %u kHz", ext_upper(tr->path),
                              tr->t.sample_rate / 1000);
    lv_obj_set_style_text_font(np_badge, pact_font_data, 0);
    lv_obj_set_style_text_color(np_badge, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_pos(np_badge, rx, 248);

    /* progress bar + times */
    np_bar = lv_bar_create(scr);
    lv_obj_set_size(np_bar, rw, 3);
    lv_obj_set_pos(np_bar, rx, 310);
    lv_bar_set_range(np_bar, 0, 100);
    lv_obj_set_style_bg_color(np_bar, PACT_COL_SELECT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(np_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(np_bar, PACT_COL_WHITE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(np_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(np_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(np_bar, 2, LV_PART_INDICATOR);

    np_elapsed = lv_label_create(scr);
    lv_obj_set_style_text_font(np_elapsed, pact_font_data, 0);
    lv_obj_set_style_text_color(np_elapsed, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_pos(np_elapsed, rx, 322);

    np_total = lv_label_create(scr);
    lv_obj_set_style_text_font(np_total, pact_font_data, 0);
    lv_obj_set_style_text_color(np_total, PACT_COL_TEXT_DIM, 0);
    lv_label_set_text(np_total, "0:00");
    lv_obj_align(np_total, LV_ALIGN_TOP_LEFT, rx + rw - 40, 322);

    np_state = lv_label_create(scr);
    lv_obj_set_style_text_font(np_state, pact_font_data, 0);
    lv_obj_set_style_text_color(np_state, PACT_COL_TEXT_DIM, 0);
    lv_label_set_text(np_state, "");
    lv_obj_set_pos(np_state, rx, 348);

    np_vol = lv_label_create(scr);
    lv_obj_set_style_text_font(np_vol, pact_font_data, 0);
    lv_obj_set_style_text_color(np_vol, PACT_COL_TEXT_DIM, 0);
    lv_label_set_text(np_vol, "");
    lv_obj_align(np_vol, LV_ALIGN_TOP_RIGHT, -PAD, 20);

    lv_obj_t *sink = lv_obj_create(scr);   /* invisible key receiver */
    lv_obj_set_size(sink, 1, 1);
    lv_obj_set_style_bg_opa(sink, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sink, 0, 0);

    ui_nowplaying_refresh();
    ui_bind_keys(sink);
    lv_screen_load(scr);
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
