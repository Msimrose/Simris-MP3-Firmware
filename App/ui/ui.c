/*
 * Pact MP-1 - UI core: navigation plumbing + main menu (landscape 600x450).
 *
 * Brand rules (docs/brand-ui-system.md): true-black ground, warm-white type,
 * no accent color, selection is a subtle grey fill, battery is the only
 * element allowed color (red under 15%).
 */
#include "ui_internal.h"

#define MENU_ROW_H   46
#define MENU_PAD_X   48

const library_t   *ui_lib;
ui_art_provider_t  ui_art_provider;
void             (*ui_on_play)(const char *path);
ui_screen_id_t     ui_cur_screen = UI_SCR_MENU;

static lv_group_t *group;

static const char *menu_items[] = {
    "Now Playing", "Albums", "Artists", "Songs", "Folders", "Playlists", "Settings",
};
#define MENU_COUNT ((int)(sizeof(menu_items) / sizeof(menu_items[0])))
#define MENU_IDX_ALBUMS 1

static lv_obj_t *menu_rows[MENU_COUNT];
static lv_obj_t *menu_labels[MENU_COUNT];
static int       menu_sel;

static lv_obj_t *batt_shell, *batt_fill, *batt_label;
static int       batt_pct = 84;
static bool      batt_chg = false;

/* ---- key plumbing ------------------------------------------------------ */

static void key_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    pact_event_t evt = PACT_EVT_NONE;
    switch (key) {
    case LV_KEY_DOWN:
    case LV_KEY_RIGHT:     evt = PACT_EVT_WHEEL_CW;  break;
    case LV_KEY_UP:
    case LV_KEY_LEFT:      evt = PACT_EVT_WHEEL_CCW; break;
    case LV_KEY_ENTER:     evt = PACT_EVT_CENTER;    break;
    case LV_KEY_ESC:
    case LV_KEY_BACKSPACE: evt = PACT_EVT_UP;        break; /* menu/back */
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

void ui_handle_event(pact_event_t evt)
{
    switch (ui_cur_screen) {
    case UI_SCR_MENU:   ui_menu_event(evt);   break;
    case UI_SCR_ALBUMS: ui_albums_event(evt); break;
    case UI_SCR_TRACKS: ui_tracks_event(evt); break;
    }
}

/* ---- battery (dynamic: proportional fill, red <15%, per brand doc) ----- */

static void battery_create(lv_obj_t *parent)
{
    batt_shell = lv_obj_create(parent);
    lv_obj_set_size(batt_shell, 36, 18);
    lv_obj_align(batt_shell, LV_ALIGN_TOP_RIGHT, -24, 18);
    lv_obj_set_style_bg_opa(batt_shell, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(batt_shell, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_border_width(batt_shell, 2, 0);
    lv_obj_set_style_radius(batt_shell, 4, 0);
    lv_obj_set_style_pad_all(batt_shell, 2, 0);
    lv_obj_clear_flag(batt_shell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *nub = lv_obj_create(parent);
    lv_obj_set_size(nub, 3, 8);
    lv_obj_align_to(nub, batt_shell, LV_ALIGN_OUT_RIGHT_MID, 1, 0);
    lv_obj_set_style_bg_color(nub, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nub, 0, 0);
    lv_obj_set_style_radius(nub, 1, 0);

    batt_fill = lv_obj_create(batt_shell);
    lv_obj_set_size(batt_fill, lv_pct(100), lv_pct(100));
    lv_obj_align(batt_fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(batt_fill, PACT_COL_WHITE, 0);
    lv_obj_set_style_bg_opa(batt_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(batt_fill, 0, 0);
    lv_obj_set_style_radius(batt_fill, 1, 0);

    batt_label = lv_label_create(parent);
    lv_obj_set_style_text_font(batt_label, pact_font_data, 0);
    lv_obj_set_style_text_color(batt_label, PACT_COL_TEXT_DIM, 0);
    ui_set_battery(batt_pct, batt_chg);
}

void ui_set_battery(int percent, bool charging)
{
    batt_pct = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    batt_chg = charging;
    if (!batt_fill) return;
    lv_obj_set_width(batt_fill, lv_pct(batt_pct));
    lv_obj_set_style_bg_color(batt_fill,
                              batt_pct < 15 ? PACT_COL_BATT_LOW : PACT_COL_WHITE, 0);
    lv_label_set_text_fmt(batt_label, batt_chg ? "%d%% +" : "%d%%", batt_pct);
    lv_obj_align_to(batt_label, batt_shell, LV_ALIGN_OUT_LEFT_MID, -10, 0);
}

/* ---- main menu --------------------------------------------------------- */

static void menu_paint_selection(void)
{
    for (int i = 0; i < MENU_COUNT; i++) {
        bool sel = (i == menu_sel);
        lv_obj_set_style_bg_opa(menu_rows[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(menu_labels[i],
                                    sel ? PACT_COL_TEXT : PACT_COL_TEXT_DIM, 0);
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
            ui_show_albums();
        break;
    default: break;
    }
}

void ui_show_menu(void)
{
    ui_cur_screen = UI_SCR_MENU;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, PACT_COL_GROUND, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *mark = lv_label_create(scr);
    lv_label_set_text(mark, "PACT");
    lv_obj_set_style_text_font(mark, pact_font_data, 0);
    lv_obj_set_style_text_color(mark, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_text_letter_space(mark, 3, 0);
    lv_obj_align(mark, LV_ALIGN_TOP_LEFT, MENU_PAD_X, 20);

    battery_create(scr);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 600, 450 - 64);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_left(list, MENU_PAD_X - 16, 0);
    lv_obj_set_style_pad_right(list, MENU_PAD_X - 16, 0);
    lv_obj_set_style_pad_top(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 2, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < MENU_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), MENU_ROW_H);
        lv_obj_set_style_bg_color(row, PACT_COL_SELECT, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_left(row, 16, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, menu_items[i]);
        lv_obj_set_style_text_font(lbl, &diatype_regular_22, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        menu_rows[i] = row;
        menu_labels[i] = lbl;
    }

    menu_paint_selection();
    ui_bind_keys(list);
    lv_screen_load(scr);
}

/* ---- public API -------------------------------------------------------- */

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
    ui_show_menu();
}

lv_group_t *ui_group(void)
{
    return group;
}
