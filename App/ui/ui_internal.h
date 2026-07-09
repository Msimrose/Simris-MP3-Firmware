/* Shared internals between UI screens. Not part of the public ui.h API. */
#pragma once
#include "ui.h"
#include "theme.h"
#include "../library/library.h"

typedef enum {
    UI_SCR_MENU = 0,
    UI_SCR_ALBUMS,
    UI_SCR_TRACKS,
    UI_SCR_NOWPLAYING,
} ui_screen_id_t;

#define UI_NO_TRACK ((size_t)-1)

extern const library_t   *ui_lib;
extern ui_art_provider_t  ui_art_provider;
extern void             (*ui_on_play)(const char *path);
extern ui_screen_id_t     ui_cur_screen;
extern size_t             ui_current_track;   /* UI_NO_TRACK if none */

size_t ui_album_of_track(size_t track_idx);
void   ui_play_track(size_t track_idx);       /* play + go to Now Playing */

/* Route all keys to obj (single-focus model: one receiver per screen). */
void ui_bind_keys(lv_obj_t *obj);

/* Screen builders / event handlers */
void ui_show_menu(void);
void ui_show_albums(void);
void ui_show_tracks(size_t album_idx);
void ui_show_nowplaying(void);
void ui_albums_event(pact_event_t evt);
void ui_tracks_event(pact_event_t evt);
void ui_menu_event(pact_event_t evt);
void ui_nowplaying_event(pact_event_t evt);
void ui_nowplaying_refresh(void);             /* periodic + on track change */
