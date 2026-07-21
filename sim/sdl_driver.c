/*
 * Pact MP-1 sim - custom SDL display + keyboard driver.
 *
 * Exists because LVGL's stock SDL window is not HiDPI-aware: on Retina the
 * 600x450 framebuffer gets scaled 2x (blocky) or filtered (soft). This
 * driver creates a HiDPI window at half logical size, so the framebuffer
 * maps 1:1 onto physical pixels: text renders exactly as sharp as the
 * device panel will show it, in a window close to the device's real size.
 * --big restores the 2x nearest-neighbor window for screen-share/demo use.
 */
#include "sdl_driver.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

#define KEYQ_LEN 32

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static uint8_t      *fb;
static int32_t       fb_w, fb_h;

static struct { uint32_t key; bool pressed; } keyq[KEYQ_LEN];
static int kq_head, kq_tail;

static void kq_push(uint32_t key, bool pressed)
{
    int next = (kq_head + 1) % KEYQ_LEN;
    if (next == kq_tail) return;  /* full: drop */
    keyq[kq_head].key = key;
    keyq[kq_head].pressed = pressed;
    kq_head = next;
}

static uint32_t map_key(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP:        return LV_KEY_UP;
    case SDLK_DOWN:      return LV_KEY_DOWN;
    case SDLK_LEFT:      return LV_KEY_LEFT;
    case SDLK_RIGHT:     return LV_KEY_RIGHT;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:  return LV_KEY_ENTER;
    case SDLK_ESCAPE:    return LV_KEY_ESC;
    case SDLK_BACKSPACE: return LV_KEY_BACKSPACE;
    default:
        if (k >= 0x20 && k < 0x7F) return (uint32_t)k;  /* ascii passthrough */
        return 0;
    }
}

static void pump_sdl_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            exit(0);
        case SDL_KEYDOWN: {
            uint32_t k = map_key(e.key.keysym.sym);
            if (k) kq_push(k, true);
            break;
        }
        case SDL_KEYUP: {
            uint32_t k = map_key(e.key.keysym.sym);
            if (k) kq_push(k, false);
            break;
        }
        default: break;
        }
    }
}

static void kb_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    pump_sdl_events();

    static uint32_t last_key;
    static bool last_state;
    if (kq_tail != kq_head) {
        last_key = keyq[kq_tail].key;
        last_state = keyq[kq_tail].pressed;
        kq_tail = (kq_tail + 1) % KEYQ_LEN;
        data->continue_reading = (kq_tail != kq_head);
    }
    data->key = last_key;
    data->state = last_state ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    if (lv_display_flush_is_last(disp)) {
        SDL_UpdateTexture(tex, NULL, fb, fb_w * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    lv_display_flush_ready(disp);
}

lv_display_t *pact_sdl_display_create(int32_t w, int32_t h, bool big)
{
    fb_w = w;
    fb_h = h;

    SDL_InitSubSystem(SDL_INIT_VIDEO);

    /* crisp mode: half-point window + HiDPI = 1:1 physical pixels on Retina;
     * big mode: full-point window, nearest-neighbor 2x */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    int win_w = big ? w : w / 2;
    int win_h = big ? h : h / 2;
    win = SDL_CreateWindow("Simris Audio Player", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, win_w, win_h,
                           big ? 0 : SDL_WINDOW_ALLOW_HIGHDPI);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, w, h);

    fb = malloc((size_t)w * h * 4);
    memset(fb, 0, (size_t)w * h * 4);

    lv_display_t *disp = lv_display_create(w, h);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(disp, fb, NULL, (uint32_t)(w * h * 4),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);
    return disp;
}

lv_indev_t *pact_sdl_keyboard_create(void)
{
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, kb_read_cb);
    return indev;
}
