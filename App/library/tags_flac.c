/* FLAC metadata: STREAMINFO + VORBIS_COMMENT + PICTURE, direct block walk. */
#include "tags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
static uint32_t le32(const uint8_t *p) {
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) | p[0];
}

static void set_str(char *dst, size_t cap, const char *src, size_t len)
{
    if (len >= cap) len = cap - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void parse_vorbis_comment(const uint8_t *d, uint32_t len, track_tags_t *t)
{
    uint32_t pos = 0;
    if (pos + 4 > len) return;
    uint32_t vendor = le32(d + pos); pos += 4;
    if (pos + vendor > len) return;
    pos += vendor;
    if (pos + 4 > len) return;
    uint32_t count = le32(d + pos); pos += 4;

    for (uint32_t i = 0; i < count && pos + 4 <= len; i++) {
        uint32_t clen = le32(d + pos); pos += 4;
        if (pos + clen > len) return;
        const char *c = (const char *)(d + pos);

        if (clen > 6 && strncasecmp(c, "TITLE=", 6) == 0)
            set_str(t->title, TAG_STR_MAX, c + 6, clen - 6);
        else if (clen > 7 && strncasecmp(c, "ARTIST=", 7) == 0)
            set_str(t->artist, TAG_STR_MAX, c + 7, clen - 7);
        else if (clen > 6 && strncasecmp(c, "ALBUM=", 6) == 0)
            set_str(t->album, TAG_STR_MAX, c + 6, clen - 6);
        else if (clen > 12 && strncasecmp(c, "TRACKNUMBER=", 12) == 0) {
            char num[8] = {0};
            set_str(num, sizeof(num), c + 12, clen - 12);
            t->track_no = (uint16_t)atoi(num);
        }
        pos += clen;
    }
}

/* PICTURE block; fills art fields. block_file_off = file offset of the
 * block payload (after the 4-byte block header). */
static void parse_picture(const uint8_t *d, uint32_t len, uint64_t block_file_off,
                          track_tags_t *t, bool *have_front)
{
    uint32_t pos = 0;
    if (pos + 8 > len) return;
    uint32_t type = be32(d + pos); pos += 4;
    uint32_t mlen = be32(d + pos); pos += 4;
    if (pos + mlen + 4 > len) return;

    char mime[TAG_MIME_MAX];
    set_str(mime, sizeof(mime), (const char *)(d + pos), mlen);
    pos += mlen;

    uint32_t dlen = be32(d + pos); pos += 4;   /* description */
    if (pos + dlen + 20 > len) return;
    pos += dlen + 16;                          /* w,h,depth,colors */
    uint32_t isize = be32(d + pos); pos += 4;
    if (pos + isize > len) return;

    /* prefer front cover (type 3); otherwise first picture wins */
    if (t->art_kind == ART_EMBEDDED && *have_front) return;
    if (t->art_kind == ART_EMBEDDED && type != 3) return;

    t->art_kind = ART_EMBEDDED;
    t->art_offset = block_file_off + pos;
    t->art_size = isize;
    strncpy(t->art_mime, mime, TAG_MIME_MAX - 1);
    if (type == 3) *have_front = true;
}

bool tags_read_flac(const char *path, track_tags_t *t)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    uint8_t hdr[4];
    if (fread(hdr, 1, 4, f) != 4 || memcmp(hdr, "fLaC", 4) != 0) {
        fclose(f);
        return false;
    }

    bool have_front = false;
    bool last = false;
    uint64_t off = 4;
    for (int guard = 0; !last && guard < 64; guard++) {
        uint8_t bh[4];
        if (fread(bh, 1, 4, f) != 4) break;
        last = bh[0] & 0x80;
        uint8_t type = bh[0] & 0x7F;
        uint32_t blen = ((uint32_t)bh[1] << 16) | ((uint32_t)bh[2] << 8) | bh[3];
        uint64_t payload_off = off + 4;

        if (type == 0 && blen >= 34) {                 /* STREAMINFO */
            uint8_t si[34];
            if (fread(si, 1, 34, f) != 34) break;
            uint32_t rate = ((uint32_t)si[10] << 12) | ((uint32_t)si[11] << 4) |
                            (si[12] >> 4);
            uint8_t bps = (uint8_t)((((si[12] & 0x01) << 4) | (si[13] >> 4)) + 1);
            uint64_t total = ((uint64_t)(si[13] & 0x0F) << 32) | be32(si + 14);
            t->sample_rate = rate;
            t->bits_per_sample = bps;
            if (rate)
                t->duration_ms = (uint32_t)(total * 1000 / rate);
            if (blen > 34) fseek(f, (long)(blen - 34), SEEK_CUR);
        } else if ((type == 4 || type == 6) && blen <= (16u << 20)) {
            uint8_t *buf = malloc(blen);
            if (!buf || fread(buf, 1, blen, f) != blen) { free(buf); break; }
            if (type == 4) parse_vorbis_comment(buf, blen, t);
            else           parse_picture(buf, blen, payload_off, t, &have_front);
            free(buf);
        } else {
            if (fseek(f, (long)blen, SEEK_CUR) != 0) break;
        }
        off = payload_off + blen;
    }

    fclose(f);
    return true;
}
