/*
 * Pact MP-1 - UI core: navigation plumbing + split main menu.
 * Variant A ("recommended"): Figma 01 split layout with 01c type scale.
 *
 * Brand rules (docs/brand-ui-system.md): true-black ground, warm-white type,
 * no accent color, selection is a subtle grey fill, battery is the only
 * element allowed color (red under 15%).
 */
#include "ui_internal.h"
#include "../audio/audio_engine.h"

#define MENU_ROW_H   43
#define MENU_PAD_X   22
#define MENU_COL_W   320

const library_t   *ui_lib;
ui_art_provider_t  ui_art_provider;
void             (*ui_on_play)(const char *path);
ui_screen_id_t     ui_cur_screen = UI_SCR_MENU;
size_t             ui_current_track = UI_NO_TRACK;

static lv_group_t *group;
static lv_obj_t   *prev_scr;

static const char *menu_items[] = {
    "Now Playing", "Albums", "Artists", "Songs", "Playlists", "Settings",
    "Shuffle Songs",
};
#define MENU_COUNT ((int)(sizeof(menu_items) / sizeof(menu_items[0])))
#define MENU_IDX_NOWPLAYING 0
#define MENU_IDX_ALBUMS     1

static lv_obj_t *menu_rows[MENU_COUNT];
static lv_obj_t *menu_labels[MENU_COUNT];
static lv_obj_t *menu_chevrons[MENU_COUNT];
static int       menu_sel;

static lv_obj_t *batt_shell, *batt_fill, *batt_label;
static int       batt_pct = 84;
static bool      batt_chg = false;

/* ---- screen lifecycle --------------------------------------------------- */

lv_obj_t *ui_screen_new(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, PACT_COL_GROUND, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    batt_fill = NULL;  /* chrome belongs to the old screen until recreated */
    batt_label = NULL;
    return scr;
}

void ui_screen_show(lv_obj_t *scr)
{
    lv_screen_load(scr);
    /* async: the old screen may be mid-event-dispatch (the key handler that
     * triggered this navigation lives on it); deleting it synchronously is
     * a use-after-free when that handler unwinds */
    if (prev_scr && prev_scr != scr) lv_obj_delete_async(prev_scr);
    prev_scr = scr;
}

/* ---- shared chrome ------------------------------------------------------ */

void ui_brand_mark(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *w = lv_label_create(parent);
    lv_label_set_text(w, "simris");
    lv_obj_set_style_text_font(w, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(w, PACT_COL_TEXT, 0);
    lv_obj_set_pos(w, x, y);

    lv_obj_t *a = lv_label_create(parent);
    lv_label_set_text(a, "audio");
    lv_obj_set_style_text_font(a, &diatype_regular_16, 0);
    lv_obj_set_style_text_color(a, PACT_COL_TEXT_DIM, 0);
    lv_obj_align_to(a, w, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, 0);

    /* offset two-square mark */
    lv_obj_t *s1 = lv_obj_create(parent);
    lv_obj_set_size(s1, 4, 4);
    lv_obj_set_style_bg_color(s1, PACT_COL_TEXT, 0);
    lv_obj_set_style_bg_opa(s1, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s1, 0, 0);
    lv_obj_set_style_radius(s1, 0, 0);
    lv_obj_align_to(s1, a, LV_ALIGN_OUT_RIGHT_TOP, 6, 0);

    lv_obj_t *s2 = lv_obj_create(parent);
    lv_obj_set_size(s2, 4, 4);
    lv_obj_set_style_bg_color(s2, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(s2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s2, 0, 0);
    lv_obj_set_style_radius(s2, 0, 0);
    lv_obj_align_to(s2, s1, LV_ALIGN_OUT_BOTTOM_RIGHT, -1, 1);
}

void ui_battery_create_at(lv_obj_t *parent, int32_t y)
{
    /* Figma: 30x14 shell, 1.5px warm-white border r3, 18x8 fill, 2.5x6 nub */
    batt_shell = lv_obj_create(parent);
    lv_obj_set_size(batt_shell, 30, 14);
    lv_obj_set_pos(batt_shell, 546, y);
    lv_obj_set_style_bg_opa(batt_shell, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(batt_shell, PACT_COL_TEXT, 0);
    lv_obj_set_style_border_width(batt_shell, 1, 0);
    lv_obj_set_style_radius(batt_shell, 3, 0);
    lv_obj_set_style_pad_hor(batt_shell, 2, 0);
    lv_obj_set_style_pad_ver(batt_shell, 2, 0);
    lv_obj_clear_flag(batt_shell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *nub = lv_obj_create(parent);
    lv_obj_set_size(nub, 3, 6);
    lv_obj_set_pos(nub, 577, y + 4);
    lv_obj_set_style_bg_color(nub, PACT_COL_TEXT, 0);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nub, 0, 0);
    lv_obj_set_style_radius(nub, 1, 0);

    batt_fill = lv_obj_create(batt_shell);
    lv_obj_set_size(batt_fill, lv_pct(100), 8);
    lv_obj_align(batt_fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(batt_fill, PACT_COL_TEXT, 0);
    lv_obj_set_style_bg_opa(batt_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(batt_fill, 0, 0);
    lv_obj_set_style_radius(batt_fill, 1, 0);

    ui_set_battery(batt_pct, batt_chg);
}

void ui_battery_create(lv_obj_t *parent)
{
    ui_battery_create_at(parent, 22);
}

void ui_set_battery(int percent, bool charging)
{
    batt_pct = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    batt_chg = charging;
    if (!batt_fill || !lv_obj_is_valid(batt_fill)) return;
    lv_obj_set_width(batt_fill, lv_pct(batt_pct));
    lv_obj_set_style_bg_color(batt_fill,
                              batt_pct < 15 ? PACT_COL_BATT_LOW : PACT_COL_WHITE, 0);
    if (batt_label && lv_obj_is_valid(batt_label)) {
        lv_label_set_text_fmt(batt_label, batt_chg ? "%d%% +" : "%d%%", batt_pct);
        lv_obj_align_to(batt_label, batt_shell, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    }
}

/* ---- key plumbing ------------------------------------------------------- */

static void key_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    pact_event_t evt = PACT_EVT_NONE;
    switch (key) {
    case LV_KEY_DOWN:      evt = PACT_EVT_WHEEL_CW;  break;
    case LV_KEY_UP:        evt = PACT_EVT_WHEEL_CCW; break;
    case LV_KEY_RIGHT:     evt = PACT_EVT_RIGHT;     break; /* next  */
    case LV_KEY_LEFT:      evt = PACT_EVT_LEFT;      break; /* prev  */
    case LV_KEY_ENTER:     evt = PACT_EVT_CENTER;    break;
    case LV_KEY_ESC:
    case LV_KEY_BACKSPACE: evt = PACT_EVT_UP;        break; /* menu/back */
    case '=':
    case '+':              evt = PACT_EVT_VOL_UP;    break;
    case '-':              evt = PACT_EVT_VOL_DOWN;  break;
    default: return;
    }
    ui_handle_event(evt);
}

void ui_bind_keys(lv_obj_t *obj)
{
    lv_group_remove_all_objs(group);
    lv_group_add_obj(group, obj);
    lv_obj_add_event_cb(obj, key_cb, LV_EVENT_KEY, NULL);
    lv_group_focus_obj(obj);
}

/* ---- volume toast -------------------------------------------------------- */

static lv_obj_t *vol_toast;
static lv_timer_t *vol_toast_timer;

static void vol_toast_expire(lv_timer_t *t)
{
    (void)t;
    if (vol_toast && lv_obj_is_valid(vol_toast)) lv_obj_delete(vol_toast);
    vol_toast = NULL;
    vol_toast_timer = NULL;
}

void ui_show_vol_toast(void)
{
    if (vol_toast && lv_obj_is_valid(vol_toast)) lv_obj_delete(vol_toast);
    vol_toast = lv_obj_create(lv_screen_active());
    lv_obj_set_size(vol_toast, 140, 34);
    lv_obj_align(vol_toast, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(vol_toast, PACT_COL_SELECT, 0);
    lv_obj_set_style_bg_opa(vol_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vol_toast, 0, 0);
    lv_obj_set_style_radius(vol_toast, 8, 0);
    lv_obj_clear_flag(vol_toast, LV_OBJ_FLAG_SCROLLABLE);

    int v = audio_engine_volume();
    lv_obj_t *lbl = lv_label_create(vol_toast);
    if (v == 0) lv_label_set_text(lbl, "muted");
    else        lv_label_set_text_fmt(lbl, "volume %d", v);
    lv_obj_set_style_text_font(lbl, &diatype_regular_15, 0);
    lv_obj_set_style_text_color(lbl, v == 0 ? PACT_COL_BATT_LOW : PACT_COL_TEXT, 0);
    lv_obj_center(lbl);

    if (vol_toast_timer) lv_timer_delete(vol_toast_timer);
    vol_toast_timer = lv_timer_create(vol_toast_expire, 1200, NULL);
    lv_timer_set_repeat_count(vol_toast_timer, 1);
}

void ui_volume_step(int dir)
{
    audio_engine_set_volume(audio_engine_volume() + dir);
    ui_nowplaying_refresh();
    ui_show_vol_toast();
}

/* ---- event routing ------------------------------------------------------- */

void ui_handle_event(pact_event_t evt)
{
    if (evt == PACT_EVT_VOL_UP || evt == PACT_EVT_VOL_DOWN) {
        ui_volume_step(evt == PACT_EVT_VOL_UP ? 1 : -1);
        return;
    }
    switch (ui_cur_screen) {
    case UI_SCR_MENU:       ui_menu_event(evt);       break;
    case UI_SCR_ALBUMS:     ui_albums_event(evt);     break;
    case UI_SCR_CAROUSEL:   ui_carousel_event(evt);   break;
    case UI_SCR_TRACKS:     ui_tracks_event(evt);     break;
    case UI_SCR_NOWPLAYING: ui_nowplaying_event(evt); break;
    }
}

size_t ui_album_of_track(size_t track_idx)
{
    if (!ui_lib) return (size_t)-1;
    for (size_t i = 0; i < ui_lib->album_count; i++) {
        const album_t *al = &ui_lib->albums[i];
        if (track_idx >= al->first && track_idx < al->first + al->count)
            return i;
    }
    return (size_t)-1;
}

void ui_play_track(size_t track_idx)
{
    if (!ui_lib || track_idx >= ui_lib->count) return;

    static uint32_t last_switch;
    uint32_t now = lv_tick_get();
    if (last_switch && now - last_switch < 350) return;
    last_switch = now;

    ui_current_track = track_idx;
    if (ui_on_play) ui_on_play(ui_lib->tracks[track_idx].path);
    if (ui_cur_screen == UI_SCR_NOWPLAYING) ui_show_nowplaying();
}

/* Half-second player tick: live progress + auto-advance through the album. */
static void player_tick(lv_timer_t *t)
{
    (void)t;
    static int finish_grace;
    if (audio_engine_state() == ENGINE_FINISHED &&
        ui_current_track != UI_NO_TRACK) {
        if (++finish_grace >= 2) {
            finish_grace = 0;
            size_t alb = ui_album_of_track(ui_current_track);
            if (alb != (size_t)-1 &&
                ui_current_track + 1 <
                    ui_lib->albums[alb].first + ui_lib->albums[alb].count) {
                ui_play_track(ui_current_track + 1);
                if (ui_cur_screen == UI_SCR_NOWPLAYING) ui_show_nowplaying();
            }
        }
    } else {
        finish_grace = 0;
    }
    ui_nowplaying_refresh();
}

/* ---- split main menu (Figma 01 layout, 01c type scale) ------------------ */

static void menu_paint_selection(void)
{
    /* Figma 01: every row warm white; selection = #1a1a1a bg + Medium cut */
    for (int i = 0; i < MENU_COUNT; i++) {
        bool sel = (i == menu_sel);
        lv_obj_set_style_bg_opa(menu_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_font(menu_labels[i],
                                   sel ? &diatype_medium_19 : &diatype_regular_19, 0);
    }
    lv_obj_scroll_to_view(menu_rows[menu_sel], LV_ANIM_ON);
}

void ui_menu_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:
        if (menu_sel < MENU_COUNT - 1) { menu_sel++; menu_paint_selection(); }
        break;
    case PACT_EVT_WHEEL_CCW:
        if (menu_sel > 0) { menu_sel--; menu_paint_selection(); }
        break;
    case PACT_EVT_CENTER:
        if (menu_sel == MENU_IDX_ALBUMS && ui_lib && ui_lib->album_count)
            ui_show_carousel();
        else if (menu_sel == MENU_IDX_NOWPLAYING && ui_current_track != UI_NO_TRACK)
            ui_show_nowplaying();
        break;
    default: break;
    }
}

void ui_show_menu(void)
{
    ui_cur_screen = UI_SCR_MENU;
    lv_obj_t *scr = ui_screen_new();

    /* header: centered "Music" Medium 18 + battery, 12% hairline below */
    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, "Music");
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

    /* left column: menu list */
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, MENU_COL_W, 450 - 72);
    lv_obj_set_pos(list, 0, 72);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 2, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < MENU_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), MENU_ROW_H);
        lv_obj_set_style_bg_color(row, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_left(row, 22, 0);
        lv_obj_set_style_pad_right(row, 18, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, menu_items[i]);
        lv_obj_set_style_text_font(lbl, &diatype_regular_19, 0);
        lv_obj_set_style_text_color(lbl, PACT_COL_TEXT, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *chev = lv_label_create(row);
        lv_label_set_text(chev, "\xE2\x80\xBA");   /* U+203A */
        lv_obj_set_style_text_font(chev, &diatype_regular_20, 0);
        lv_obj_set_style_text_color(chev, PACT_COL_TEXT_DIM, 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);

        menu_rows[i] = row;
        menu_labels[i] = lbl;
        menu_chevrons[i] = chev;
    }

    /* right pane: current (or first) album, art-forward */
    size_t alb = (size_t)-1;
    const char *pane_title = NULL, *pane_sub = NULL;
    if (ui_current_track != UI_NO_TRACK) {
        alb = ui_album_of_track(ui_current_track);
        pane_title = ui_lib->tracks[ui_current_track].t.title;
        pane_sub = ui_lib->tracks[ui_current_track].t.artist;
    } else if (ui_lib && ui_lib->album_count) {
        alb = 0;
        pane_title = ui_lib->albums[0].album;
        pane_sub = ui_lib->albums[0].artist;
    }
    if (alb != (size_t)-1) {
        const char *art = ui_art_provider ? ui_art_provider(alb, 190) : NULL;
        if (art) {
            lv_obj_t *cov = lv_image_create(scr);
            lv_image_set_src(cov, art);
            lv_obj_set_size(cov, 190, 190);
            lv_obj_set_pos(cov, 365, 100);
            lv_obj_set_style_radius(cov, 5, 0);
            lv_obj_set_style_clip_corner(cov, true, 0);
        }
        lv_obj_t *t1 = lv_label_create(scr);
        lv_label_set_text(t1, pane_title ? pane_title : "");
        lv_obj_set_style_text_font(t1, &diatype_medium_16, 0);
        lv_obj_set_style_text_color(t1, PACT_COL_TEXT, 0);
        lv_label_set_long_mode(t1, LV_LABEL_LONG_DOT);
        lv_obj_set_size(t1, 210, 20);
        lv_obj_set_pos(t1, 365, 308);

        lv_obj_t *t2 = lv_label_create(scr);
        lv_label_set_text(t2, pane_sub ? pane_sub : "");
        lv_obj_set_style_text_font(t2, &diatype_light_13, 0);
        lv_obj_set_style_text_color(t2, PACT_COL_TEXT_DIM, 0);
        lv_label_set_long_mode(t2, LV_LABEL_LONG_DOT);
        lv_obj_set_size(t2, 210, 18);
        lv_obj_set_pos(t2, 365, 330);
    }

    menu_paint_selection();
    ui_bind_keys(list);
    ui_screen_show(scr);
}

/* ---- public API ---------------------------------------------------------- */

void ui_set_library(const library_t *lib, ui_art_provider_t art)
{
    ui_lib = lib;
    ui_art_provider = art;
}

void ui_set_on_play(void (*fn)(const char *path))
{
    ui_on_play = fn;
}

void ui_init(void)
{
    group = lv_group_create();
    lv_timer_create(player_tick, 500, NULL);
    ui_show_menu();
}

lv_group_t *ui_group(void)
{
    return group;
}
