/*
 * Pact MP-1 - thumb cache builder (device + fatfs_test; the sim has its
 * own sips pipeline, so this whole file is !PACT_SIM).
 *
 * Pipeline per album+size: open the art source through pact_io (embedded
 * art = a byte window inside the track file; folder art = the whole file),
 * TJPGD-decode with the largest 1/2^n prescale that still covers the
 * target, center-crop square, nearest-neighbor map into a full RGB888
 * target buffer on the heap (232px worst case = 162K; the AXI arena is
 * sized for it), and stream out a classic bottom-up 24-bit BMP.
 *
 * Nearest sampling is bring-up quality; if it reads as aliased on the
 * panel, the upgrade path is box-filtering here or DMA2D bilinear at
 * Phase-8 - the cache regenerates when PACT_THUMB_VERSION bumps.
 */
#ifndef PACT_SIM

#include "thumbcache.h"
#include "../pact_io.h"
#include "ff.h"                       /* f_mkdir - !PACT_SIM is always FatFs */
#include "src/libs/tjpgd/tjpgd.h"     /* via the lib/lvgl include root */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PACT_THUMB_VERSION 1          /* bump to force a cache rebuild */

const int pact_thumb_sizes[PACT_THUMB_SIZE_COUNT] = { 56, 180, 190, 232 };

/* djb2, same keying as the sim art cache */
static uint32_t path_hash(const char *s)
{
    uint32_t h = 5381;
    while (*s) h = h * 33u + (uint8_t)*s++;
    return h;
}

static void thumb_path(char *buf, size_t len, const char *dir, uint32_t key,
                       int px)
{
    snprintf(buf, len, "%s/%08x_%d.bmp", dir, (unsigned)key, px);
}

/* ---- decode state --------------------------------------------------------- */

typedef struct {
    /* input window */
    pact_file_t *f;
    uint64_t     base;
    uint64_t     len;
    uint64_t     pos;       /* relative to base */
    /* output */
    int          dst;       /* px */
    uint8_t     *img;       /* dst*dst*3, RGB888 */
    uint16_t    *map_x;     /* dst coord -> scaled-src coord, per axis     */
    uint16_t    *map_y;     /* (center-crop offsets folded in)             */
} dec_t;

static size_t jpg_in(JDEC *jd, uint8_t *buf, size_t n)
{
    dec_t *d = (dec_t *)jd->device;
    if (d->pos >= d->len) return 0;
    if (d->pos + n > d->len) n = (size_t)(d->len - d->pos);
    if (buf) {
        if (!pact_seek(d->f, d->base + d->pos)) return 0;
        n = pact_read(d->f, buf, n);
    }
    d->pos += n;
    return n;
}

/* first dst index whose mapped coord is >= v (map is monotonic) */
static int map_lower(const uint16_t *map, int n, int v)
{
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (map[mid] < v) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static int jpg_out(JDEC *jd, void *bitmap, JRECT *rect)
{
    dec_t *d = (dec_t *)jd->device;
    const uint8_t *px = (const uint8_t *)bitmap;
    int rw = rect->right - rect->left + 1;

    int y0 = map_lower(d->map_y, d->dst, rect->top);
    int x0 = map_lower(d->map_x, d->dst, rect->left);
    for (int dy = y0; dy < d->dst && d->map_y[dy] <= rect->bottom; dy++) {
        uint8_t *dst_row = d->img + (size_t)dy * d->dst * 3;
        int sy = d->map_y[dy] - rect->top;
        for (int dx = x0; dx < d->dst && d->map_x[dx] <= rect->right; dx++) {
            int sx = d->map_x[dx] - rect->left;
            const uint8_t *s = px + ((size_t)sy * rw + sx) * 3;
            uint8_t *o = dst_row + (size_t)dx * 3;
            o[0] = s[0]; o[1] = s[1]; o[2] = s[2];
        }
    }
    return 1;
}

/* ---- BMP writer (24-bit, bottom-up, classic 54-byte header) ---------------- */

static void w32(uint8_t *p, uint32_t v)
{
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static bool write_bmp(const char *path, const uint8_t *rgb, int px)
{
    uint32_t stride = (((uint32_t)px * 3) + 3u) & ~3u;
    uint32_t img_sz = stride * (uint32_t)px;

    uint8_t h[54] = { 'B', 'M' };
    w32(h + 2, 54 + img_sz);
    w32(h + 10, 54);
    w32(h + 14, 40);
    w32(h + 18, (uint32_t)px);
    w32(h + 22, (uint32_t)px);       /* positive height = bottom-up */
    h[26] = 1;                        /* planes */
    h[28] = 24;                       /* bpp */
    w32(h + 34, img_sz);
    w32(h + 38, 2835); w32(h + 42, 2835);

    uint8_t *row = malloc(stride);
    if (!row) return false;
    memset(row, 0, stride);

    pact_file_t *f = pact_open_write(path);
    if (!f) { free(row); return false; }

    bool ok = pact_write(f, h, 54) == 54;
    for (int y = px - 1; ok && y >= 0; y--) {          /* bottom-up */
        const uint8_t *src = rgb + (size_t)y * px * 3;
        for (int x = 0; x < px; x++) {                  /* RGB -> BGR */
            row[x * 3 + 0] = src[x * 3 + 2];
            row[x * 3 + 1] = src[x * 3 + 1];
            row[x * 3 + 2] = src[x * 3 + 0];
        }
        ok = pact_write(f, row, stride) == stride;
    }
    pact_close(f);
    free(row);
    return ok;
}

/* ---- one thumb -------------------------------------------------------------- */

static bool build_one(const track_t *tr, const char *out_path, int px)
{
    dec_t d = { .dst = px };

    if (tr->t.art_kind == ART_EMBEDDED) {
        d.f = pact_open(tr->path);
        d.base = tr->t.art_offset;
        d.len = tr->t.art_size;
    } else if (tr->t.art_kind == ART_FOLDER_FILE && tr->folder_art) {
        d.f = pact_open(tr->folder_art);
        d.base = 0;
        d.len = d.f ? pact_size(d.f) : 0;
    }
    if (!d.f) return false;

    bool ok = false;
    void *pool = malloc(8192);                       /* TJPGD workspace */
    JDEC jd;
    if (pool && jd_prepare(&jd, jpg_in, pool, 8192, &d) == JDR_OK) {
        /* Full-resolution decode, downscale in the sampling maps. LVGL's
         * vendored tjpgdcnf.h has JD_USE_SCALE=0 and lvgl is a pristine
         * submodule we do not patch; the 1/2^n prescale would only save
         * scan-time CPU (one-time per album). If first-boot thumb builds
         * measure too slow on the H7, that is the knob to revisit. */
        int sw = jd.width, sh = jd.height;
        int side = sw < sh ? sw : sh;         /* centered square crop */
        int ox = (sw - side) / 2, oy = (sh - side) / 2;

        d.img   = malloc((size_t)px * px * 3);
        d.map_x = malloc(sizeof(uint16_t) * (size_t)px);
        d.map_y = malloc(sizeof(uint16_t) * (size_t)px);
        if (d.img && d.map_x && d.map_y) {
            /* dst -> scaled-source coordinate, center-of-pixel nearest */
            for (int i = 0; i < px; i++) {
                uint16_t m = (uint16_t)(((2 * i + 1) * side) / (2 * px));
                d.map_x[i] = (uint16_t)(ox + m);
                d.map_y[i] = (uint16_t)(oy + m);
            }
            if (jd_decomp(&jd, jpg_out, 0) == JDR_OK)
                ok = write_bmp(out_path, d.img, px);
        }
        free(d.img);
        free(d.map_x);
        free(d.map_y);
    }
    free(pool);
    pact_close(d.f);
    return ok;
}

/* ---- public ------------------------------------------------------------------- */

size_t thumbcache_build(const library_t *lib, const char *dir)
{
    f_mkdir(dir);                    /* FR_EXIST is fine */

    size_t built = 0;
    for (size_t a = 0; a < lib->album_count; a++) {
        const track_t *tr = &lib->tracks[lib->albums[a].first];
        if (tr->t.art_kind == ART_NONE) continue;
        uint32_t key = path_hash(tr->path);
        for (int s = 0; s < PACT_THUMB_SIZE_COUNT; s++) {
            char path[96];
            thumb_path(path, sizeof path, dir, key, pact_thumb_sizes[s]);
            pact_file_t *exist = pact_open(path);
            if (exist) { pact_close(exist); continue; }
            if (build_one(tr, path, pact_thumb_sizes[s]))
                built++;
        }
    }
    return built;
}

const char *thumbcache_file(const library_t *lib, size_t album_idx, int px,
                            const char *dir, char *buf, size_t buflen)
{
    if (album_idx >= lib->album_count) return NULL;
    const track_t *tr = &lib->tracks[lib->albums[album_idx].first];
    if (tr->t.art_kind == ART_NONE) return NULL;
    thumb_path(buf, buflen, dir, path_hash(tr->path), px);
    pact_file_t *f = pact_open(buf);
    if (!f) return NULL;
    pact_close(f);
    return buf;
}

#endif /* !PACT_SIM */
