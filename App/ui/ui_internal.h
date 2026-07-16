/* Shared internals between UI screens. Not part of the public ui.h API. */
#pragma once
#include "ui.h"
#include "theme.h"
#include "../library/library.h"

typedef enum {
    UI_SCR_MENU = 0,
    UI_SCR_ALBUMS,
    UI_SCR_CAROUSEL,
    UI_SCR_TRACKS,
    UI_SCR_NOWPLAYING,
    UI_SCR_ARTISTS,
    UI_SCR_ARTIST,
    UI_SCR_SONGS,
    UI_SCR_SETTINGS,
    UI_SCR_GRID,
} ui_screen_id_t;

#define UI_NO_TRACK ((size_t)-1)

extern const library_t   *ui_lib;
extern ui_art_provider_t  ui_art_provider;
extern void             (*ui_on_play)(const char *path);
extern ui_screen_id_t     ui_cur_screen;
extern size_t             ui_current_track;   /* UI_NO_TRACK if none */

size_t ui_album_of_track(size_t track_idx);
void   ui_play_track(size_t track_idx);       /* rate-limited play */
void   ui_volume_step(int dir);               /* +-1 step, shows toast */
void   ui_requeue_next(void);                 /* re-sync engine gapless queue */

/* Route all keys to obj (single-focus model: one receiver per screen). */
void ui_bind_keys(lv_obj_t *obj);

/* Screen lifecycle: create a black screen, then show it (deletes the
 * previous screen so navigation cannot leak). */
lv_obj_t *ui_screen_new(void);
void      ui_screen_show(lv_obj_t *scr);

/* Shared chrome */
void ui_battery_create(lv_obj_t *parent);          /* top-right battery, y=22 */
void ui_battery_create_at(lv_obj_t *parent, int32_t y);
void ui_brand_mark(lv_obj_t *parent, int x, int y); /* simris audio mark */

/* Screen builders / event handlers */
void ui_show_menu(void);
void ui_show_albums(void);
void ui_show_carousel(void);
void ui_show_tracks(size_t album_idx);
void ui_show_nowplaying(void);
void ui_show_artists(void);
void ui_show_artist(size_t artist_idx);
void ui_show_songs(void);
void ui_show_settings(void);
void ui_show_grid(void);
void ui_albums_event(pact_event_t evt);
void ui_carousel_event(pact_event_t evt);
void ui_tracks_event(pact_event_t evt);
void ui_menu_event(pact_event_t evt);
void ui_nowplaying_event(pact_event_t evt);
void ui_artists_event(pact_event_t evt);
void ui_artist_event(pact_event_t evt);
void ui_songs_event(pact_event_t evt);
void ui_settings_event(pact_event_t evt);
void ui_grid_event(pact_event_t evt);

/* where the tracks screen's back button returns (set before ui_show_tracks) */
extern ui_screen_id_t ui_tracks_back;
extern size_t         ui_tracks_back_artist;
void ui_nowplaying_refresh(void);             /* periodic + on track change */
