/*
 * Pact MP-1 - UI skeleton: main menu (landscape 600x450).
 *
 * Brand rules (docs/brand-ui-system.md): true-black ground, warm-white type,
 * no accent color, selection is a subtle grey fill, battery is the only
 * element allowed color (red under 15%).
 */
#include "ui.h"
#include "theme.h"

#define MENU_ROW_H   46
#define MENU_PAD_X   48

static const char *menu_items[] = {
    "Now Playing", "Albums", "Artists", "Songs", "Folders", "Playlists", "Settings",
};
#define MENU_COUNT ((int)(sizeof(menu_items) / sizeof(menu_items[0])))

static lv_group_t *group;
static lv_obj_t   *menu_rows[MENU_COUNT];
static lv_obj_t   *menu_labels[MENU_COUNT];
static int         menu_sel;

static lv_obj_t *batt_shell;
static lv_obj_t *batt_fill;
static lv_obj_t *batt_label;

/* ---- battery (dynamic: proportional fill, red <15%, per brand doc) ---- */

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

    /* nub */
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
    lv_obj_set_style_text_font(batt_label, &scotch_mono_16, 0);
    lv_obj_set_style_text_color(batt_label, PACT_COL_TEXT_DIM, 0);
    lv_obj_align_to(batt_label, batt_shell, LV_ALIGN_OUT_LEFT_MID, -10, 0);
}

void ui_set_battery(int percent, bool charging)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    lv_obj_set_width(batt_fill, lv_pct(percent));
    lv_obj_set_style_bg_color(batt_fill,
                              percent < 15 ? PACT_COL_BATT_LOW : PACT_COL_WHITE, 0);
    lv_label_set_text_fmt(batt_label, charging ? "%d%% +" : "%d%%", percent);
    lv_obj_align_to(batt_label, batt_shell, LV_ALIGN_OUT_LEFT_MID, -10, 0);
}

/* ---- menu ---- */

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

static void menu_move(int dir)
{
    menu_sel += dir;
    if (menu_sel < 0) menu_sel = 0;
    if (menu_sel >= MENU_COUNT) menu_sel = MENU_COUNT - 1;
    menu_paint_selection();
}

void ui_handle_event(pact_event_t evt)
{
    switch (evt) {
    case PACT_EVT_WHEEL_CW:  menu_move(+1); break;
    case PACT_EVT_WHEEL_CCW: menu_move(-1); break;
    case PACT_EVT_CENTER:    /* select: screens beyond the menu come next */ break;
    default: break;
    }
}

/* LVGL keypad plumbing -> semantic events (device uses the same path:
 * hal feeds LV_KEY_* from buttons/wheel; the app only sees pact_event_t). */
static void menu_key_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    switch (key) {
    case LV_KEY_DOWN:
    case LV_KEY_RIGHT: ui_handle_event(PACT_EVT_WHEEL_CW);  break;
    case LV_KEY_UP:
    case LV_KEY_LEFT:  ui_handle_event(PACT_EVT_WHEEL_CCW); break;
    case LV_KEY_ENTER: ui_handle_event(PACT_EVT_CENTER);    break;
    default: break;
    }
}

void ui_init(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, PACT_COL_GROUND, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* wordmark, quiet, top-left */
    lv_obj_t *mark = lv_label_create(scr);
    lv_label_set_text(mark, "PACT");
    lv_obj_set_style_text_font(mark, &scotch_mono_16, 0);
    lv_obj_set_style_text_color(mark, PACT_COL_TEXT_DIM, 0);
    lv_obj_set_style_text_letter_space(mark, 3, 0);
    lv_obj_align(mark, LV_ALIGN_TOP_LEFT, MENU_PAD_X, 20);

    battery_create(scr);

    /* menu list */
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

    /* input group: one focus holder receives keys, we translate to events */
    group = lv_group_create();
    lv_group_add_obj(group, list);
    lv_obj_add_event_cb(list, menu_key_cb, LV_EVENT_KEY, NULL);

    menu_sel = 0;
    menu_paint_selection();
    ui_set_battery(84, false);
}

lv_group_t *ui_group(void)
{
    return group;
}
