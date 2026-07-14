/*
 * Pact MP-1 - desktop simulator (SDL2).
 *
 * Renders the real UI at 600x450 and plays real audio through the real
 * engine. Keys stand in for the hardware:
 *   arrows        = wheel
 *   enter         = center press (select / play)
 *   esc/backspace = menu-top button (back)
 *
 * Flags:
 *   --library <dir>   music folder to scan (default /tmp/pact-demo)
 *   --play <file>     start playing a file immediately
 *   --datafont scotch|slab|diatype
 *   --shot <out.bmp>  render ~0.7 s, save screenshot, exit
 *   --screen albums   with --shot: jump to a screen first
 */
#include "lvgl.h"
#include "sdl_driver.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../App/ui/ui.h"
#include "../App/ui/theme.h"
#include "../App/audio/audio_engine.h"
#include "../App/audio/pcm_ring.h"
#include "../App/library/library.h"

/* ---- audio output: SDL stands in for the SAI DMA ring consumer ---------- */

#define SIM_RING_FRAMES 16384  /* ~370 ms at 44.1 kHz */
static int32_t           ring_storage[SIM_RING_FRAMES * 2];
static pcm_ring_t        ring;
static SDL_AudioDeviceID adev;
static int               adev_rate;

static void sdl_audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    size_t want = (size_t)len / (2 * sizeof(int32_t));
    size_t got = pcm_ring_read(&ring, (int32_t *)stream, want);
    if (got < want)  /* underrun: silence, never stale samples */
        memset(stream + got * 8, 0, (want - got) * 8);
}

static int pump_thread(void *ud)
{
    (void)ud;
    for (;;) {
        if (!audio_engine_pump()) SDL_Delay(3);
    }
    return 0;
}

static void audio_init_once(void)
{
    static bool done;
    if (done) return;
    done = true;
    pcm_ring_init(&ring, ring_storage, SIM_RING_FRAMES);
    audio_engine_init(&ring);
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_CreateThread(pump_thread, "audio_pump", NULL);
}

/* Play a track; reopen the output device if the sample rate changed
 * (the sim equivalent of switching the SAI kernel PLL per track). */
static void sim_play(const char *path)
{
    audio_init_once();
    if (!audio_engine_play(path)) {
        fprintf(stderr, "cannot open %s\n", path);
        return;
    }
    const audio_fmt_t *fmt = audio_engine_fmt();
    printf("playing: %s (%u Hz, %u bit)\n", path, fmt->sample_rate,
           fmt->bits_per_sample);

    if (!adev || adev_rate != (int)fmt->sample_rate) {
        if (adev) SDL_CloseAudioDevice(adev);
        SDL_AudioSpec want = {0}, have;
        want.freq = (int)fmt->sample_rate;
        want.format = AUDIO_S32SYS;
        want.channels = 2;
        want.samples = 1024;
        want.callback = sdl_audio_cb;
        adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        adev_rate = (int)fmt->sample_rate;
        if (!adev) {
            fprintf(stderr, "audio device: %s\n", SDL_GetError());
            return;
        }
    }
    SDL_PauseAudioDevice(adev, 0);
}

/* ---- album art provider: extract + thumbnail to /tmp/pact_art ----------- */

static library_t lib;

static uint32_t path_hash(const char *s)
{
    uint32_t h = 5381;
    while (*s) h = h * 33u + (uint8_t)*s++;
    return h;
}

static const char *sim_album_art(size_t album_idx, int px)
{
    static char lv_path[512];
    if (album_idx >= lib.album_count) return NULL;

    const track_t *tr = &lib.tracks[lib.albums[album_idx].first];
    if (tr->t.art_kind == ART_NONE) return NULL;

    uint32_t key = path_hash(tr->path);   /* cache survives library switches */
    mkdir("/tmp/pact_art", 0755);
    char full[512], thumb[512];
    snprintf(full, sizeof(full), "/tmp/pact_art/%08x.img", key);
    snprintf(thumb, sizeof(thumb), "/tmp/pact_art/%08x_%d.jpg", key, px);

    char thumb_bmp[512];
    snprintf(thumb_bmp, sizeof(thumb_bmp), "/tmp/pact_art/%08x_%d.bmp", key, px);

    struct stat st;
    if (stat(thumb, &st) != 0) {  /* build once per size, then reuse */
        if (stat(full, &st) != 0 && !library_extract_art(tr, full)) return NULL;
        char cmd[1200];
        snprintf(cmd, sizeof(cmd),
                 "sips -s format jpeg -z %d %d '%s' --out '%s' >/dev/null 2>&1",
                 px, px, full, thumb);
        if (system(cmd) != 0 || stat(thumb, &st) != 0) return NULL;
    }
    if (stat(thumb_bmp, &st) != 0) {  /* raw twin for RAM/scale use */
        char cmd[1200];
        snprintf(cmd, sizeof(cmd),
                 "sips -s format bmp -z %d %d '%s' --out '%s' >/dev/null 2>&1",
                 px, px, full, thumb_bmp);
        (void)system(cmd);
    }

    snprintf(lv_path, sizeof(lv_path), "A:%s", thumb);
    return lv_path;
}

/* ---- screenshot (--shot) ------------------------------------------------ */

static int write_bmp(const char *path, const lv_draw_buf_t *buf)
{
    const uint32_t w = buf->header.w, h = buf->header.h;
    const uint32_t stride = buf->header.stride;
    const uint32_t img_bytes = w * h * 4;
    uint8_t hdr[54] = {0};

    hdr[0] = 'B'; hdr[1] = 'M';
    uint32_t fsize = 54 + img_bytes;
    memcpy(hdr + 2, &fsize, 4);
    uint32_t off = 54;             memcpy(hdr + 10, &off, 4);
    uint32_t bisize = 40;          memcpy(hdr + 14, &bisize, 4);
    int32_t  bw = (int32_t)w;      memcpy(hdr + 18, &bw, 4);
    int32_t  bh = -(int32_t)h;     memcpy(hdr + 22, &bh, 4); /* top-down */
    uint16_t planes = 1;           memcpy(hdr + 26, &planes, 2);
    uint16_t bpp = 32;             memcpy(hdr + 28, &bpp, 2);
    memcpy(hdr + 34, &img_bytes, 4);

    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(hdr, 1, 54, f);
    for (uint32_t y = 0; y < h; y++)
        fwrite((const uint8_t *)buf->data + y * stride, 1, w * 4, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    const char *shot_path = NULL;
    const char *play_path = NULL;
    const char *lib_dir = NULL;
    const char *jump_screen = NULL;
    bool big_window = false;
    static char saved_lib[512];

    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--big") == 0) big_window = true;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--shot") == 0)    shot_path = argv[i + 1];
        if (strcmp(argv[i], "--play") == 0)    play_path = argv[i + 1];
        if (strcmp(argv[i], "--library") == 0) lib_dir = argv[i + 1];
        if (strcmp(argv[i], "--screen") == 0)  jump_screen = argv[i + 1];

        if (strcmp(argv[i], "--datafont") == 0) {
            const char *f = argv[i + 1];
            if      (strcmp(f, "scotch")  == 0) pact_font_data = &scotch_mono_16;
            else if (strcmp(f, "slab")    == 0) pact_font_data = &slab_mono_16;
            else if (strcmp(f, "diatype") == 0) pact_font_data = &diatype_regular_16;
            else { fprintf(stderr, "unknown --datafont %s\n", f); return 1; }
        }
    }

    /* Remember the last library folder across runs (~/.pact_sim_lib). */
    char cfg[512];
    snprintf(cfg, sizeof(cfg), "%s/.pact_sim_lib", getenv("HOME") ?: "/tmp");
    if (lib_dir) {
        FILE *f = fopen(cfg, "w");
        if (f) { fputs(lib_dir, f); fclose(f); }
    } else {
        FILE *f = fopen(cfg, "r");
        if (f && fgets(saved_lib, sizeof(saved_lib), f)) {
            saved_lib[strcspn(saved_lib, "\n")] = 0;
            lib_dir = saved_lib;
        }
        if (f) fclose(f);
        if (!lib_dir || !lib_dir[0]) lib_dir = "/tmp/pact-demo";
    }

    lv_init();
    lv_tick_set_cb(SDL_GetTicks);

    /* HiDPI 1:1 window by default (pixel-exact, near device size);
     * --big = 2x nearest-neighbor for demos */
    lv_display_t *disp = pact_sdl_display_create(600, 450, big_window);
    (void)disp;
    lv_indev_t *kb = pact_sdl_keyboard_create();

    library_scan(&lib, lib_dir);
    printf("library: %zu tracks, %zu albums from %s\n", lib.count,
           lib.album_count, lib_dir);

    ui_set_library(&lib, sim_album_art);
    ui_set_on_play(sim_play);
    ui_init();
    lv_indev_set_group(kb, ui_group());
    lv_group_focus_next(ui_group());

    if (jump_screen && lib.album_count) {
        ui_handle_event(PACT_EVT_WHEEL_CW);   /* menu: Now Playing -> Albums */
        ui_handle_event(PACT_EVT_CENTER);     /* enter Albums */
        if (strcmp(jump_screen, "tracks") == 0 ||
            strcmp(jump_screen, "nowplaying") == 0)
            ui_handle_event(PACT_EVT_CENTER); /* open first album */
        if (strcmp(jump_screen, "nowplaying") == 0)
            ui_handle_event(PACT_EVT_CENTER); /* play track 1 -> Now Playing */
    }

    if (play_path) sim_play(play_path);

    /* --stress2: same graph but through REAL SDL key events (the path a
     * human uses; catches delete-during-event-dispatch bugs) */
    if (jump_screen && strcmp(jump_screen, "stress2") == 0) {
        SDL_Keycode script[] = {
            SDLK_DOWN, SDLK_RETURN,               /* menu -> carousel */
            SDLK_DOWN, SDLK_DOWN, SDLK_UP,
            SDLK_RETURN,                          /* -> tracks */
            SDLK_DOWN, SDLK_ESCAPE,               /* -> carousel */
            SDLK_ESCAPE,                          /* -> menu */
            SDLK_UP,
        };
        for (int round = 0; round < 20; round++) {
            for (size_t k = 0; k < sizeof(script) / sizeof(script[0]); k++) {
                SDL_Event e; memset(&e, 0, sizeof(e));
                e.type = SDL_KEYDOWN;
                e.key.keysym.sym = script[k];
                SDL_PushEvent(&e);
                for (int t = 0; t < 6; t++) { lv_timer_handler(); SDL_Delay(2); }
                e.type = SDL_KEYUP;
                SDL_PushEvent(&e);
                for (int t = 0; t < 3; t++) { lv_timer_handler(); SDL_Delay(2); }
            }
            if ((round % 5) == 0) printf("stress2 round %d ok\n", round);
        }
        printf("stress2 complete\n");
        return 0;
    }

    /* --stress: hammer the navigation graph headlessly until it breaks */
    if (jump_screen && strcmp(jump_screen, "stress") == 0) {
        pact_event_t script[] = {
            PACT_EVT_WHEEL_CW, PACT_EVT_CENTER,            /* menu -> carousel */
            PACT_EVT_WHEEL_CW, PACT_EVT_WHEEL_CW,
            PACT_EVT_WHEEL_CCW, PACT_EVT_WHEEL_CW,
            PACT_EVT_CENTER,                               /* -> tracks */
            PACT_EVT_WHEEL_CW, PACT_EVT_UP,                /* -> carousel */
            PACT_EVT_WHEEL_CCW, PACT_EVT_UP,               /* -> menu */
            PACT_EVT_WHEEL_CCW,
        };
        for (int round = 0; round < 25; round++) {
            for (size_t k = 0; k < sizeof(script) / sizeof(script[0]); k++) {
                ui_handle_event(script[k]);
                for (int t = 0; t < 8; t++) { lv_timer_handler(); SDL_Delay(2); }
            }
            if ((round % 5) == 0) printf("stress round %d ok\n", round);
        }
        printf("stress complete\n");
        return 0;
    }

    uint32_t start = SDL_GetTicks();
    while (1) {
        lv_timer_handler();
        SDL_Delay(5);
        if (shot_path && SDL_GetTicks() - start > 700) {
            lv_draw_buf_t *snap =
                lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_XRGB8888);
            if (!snap || write_bmp(shot_path, snap) != 0) {
                fprintf(stderr, "snapshot failed\n");
                return 1;
            }
            printf("wrote %s\n", shot_path);
            return 0;
        }
    }
}
