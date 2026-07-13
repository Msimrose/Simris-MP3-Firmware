#include "thumb.h"
#include "../pact_io.h"
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

lv_draw_buf_t *pact_thumb_load_bmp(const char *path)
{
    pact_file_t *f = pact_open(path);
    if (!f) return NULL;

    uint8_t hdr[54];
    if (pact_read(f, hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        pact_close(f);
        return NULL;
    }

    uint32_t data_off = rd32(hdr + 10);
    uint32_t hdr_size = rd32(hdr + 14);
    int32_t  w = (int32_t)rd32(hdr + 18);
    int32_t  h_raw = (int32_t)rd32(hdr + 22);
    uint16_t bpp = rd16(hdr + 28);
    uint32_t comp = rd32(hdr + 30);
    (void)hdr_size;

    bool top_down = h_raw < 0;
    int32_t h = top_down ? -h_raw : h_raw;

    if (w <= 0 || h <= 0 || w > 2048 || h > 2048 ||
        (bpp != 24 && bpp != 32) || comp != 0) {
        pact_close(f);
        return NULL;
    }

    uint32_t src_stride = (((uint32_t)w * bpp / 8) + 3u) & ~3u;
    uint8_t *row = malloc(src_stride);
    lv_draw_buf_t *buf =
        lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_ARGB8888, 0);
    if (!row || !buf) {
        free(row);
        if (buf) lv_draw_buf_destroy(buf);
        pact_close(f);
        return NULL;
    }

    bool ok = pact_seek(f, data_off);
    for (int32_t y = 0; ok && y < h; y++) {
        if (pact_read(f, row, src_stride) != src_stride) { ok = false; break; }
        int32_t dy = top_down ? y : (h - 1 - y);
        uint8_t *dst = buf->data + (size_t)dy * buf->header.stride;
        if (bpp == 32) {
            for (int32_t x = 0; x < w; x++) {   /* BGRA -> BGRA, force opaque */
                dst[x * 4 + 0] = row[x * 4 + 0];
                dst[x * 4 + 1] = row[x * 4 + 1];
                dst[x * 4 + 2] = row[x * 4 + 2];
                dst[x * 4 + 3] = 0xFF;
            }
        } else {
            for (int32_t x = 0; x < w; x++) {   /* BGR -> BGRA */
                dst[x * 4 + 0] = row[x * 3 + 0];
                dst[x * 4 + 1] = row[x * 3 + 1];
                dst[x * 4 + 2] = row[x * 3 + 2];
                dst[x * 4 + 3] = 0xFF;
            }
        }
    }

    free(row);
    pact_close(f);
    if (!ok) {
        lv_draw_buf_destroy(buf);
        return NULL;
    }
    return buf;
}
