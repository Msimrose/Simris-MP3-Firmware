/*
 * Pact MP-1 - desktop simulator (SDL2).
 *
 * Renders the real UI at 600x450 in a window. Keys stand in for the hardware:
 *   arrows        = wheel (up/left CCW, down/right CW)
 *   enter         = center press
 * `--shot out.bmp` renders ~0.7 s then writes a screenshot and exits
 * (used for automated visual checks).
 */
#include "lvgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../App/ui/ui.h"

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
    if (argc >= 3 && strcmp(argv[1], "--shot") == 0) shot_path = argv[2];

    lv_init();
    lv_tick_set_cb(SDL_GetTicks);

    lv_display_t *disp = lv_sdl_window_create(600, 450);
    lv_sdl_window_set_title(disp, "Pact MP-1");
    lv_indev_t *kb = lv_sdl_keyboard_create();

    ui_init();
    lv_indev_set_group(kb, ui_group());
    lv_group_focus_next(ui_group());

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
