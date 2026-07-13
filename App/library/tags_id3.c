/*
 * MP3 metadata: ID3v2.3/2.4 text frames + APIC art, ID3v1 fallback,
 * duration from Xing/Info/VBRI headers with a CBR estimate fallback.
 */
#include "tags.h"
#include "../pact_io.h"
#include <stdlib.h>
#include <string.h>

/* ---------- text encoding helpers (everything normalized to UTF-8) ------ */

static void latin1_to_utf8(char *dst, size_t cap, const uint8_t *src, size_t len)
{
    size_t o = 0;
    for (size_t i = 0; i < len && src[i]; i++) {
        uint8_t c = src[i];
        if (c < 0x80) {
            if (o + 1 >= cap) break;
            dst[o++] = (char)c;
        } else {
            if (o + 2 >= cap) break;
            dst[o++] = (char)(0xC0 | (c >> 6));
            dst[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    dst[o] = '\0';
}

static void utf16_to_utf8(char *dst, size_t cap, const uint8_t *src, size_t len,
                          bool big_endian)
{
    size_t o = 0, i = 0;
    /* BOM check */
    if (len >= 2) {
        if (src[0] == 0xFF && src[1] == 0xFE) { big_endian = false; i = 2; }
        else if (src[0] == 0xFE && src[1] == 0xFF) { big_endian = true; i = 2; }
    }
    while (i + 1 < len) {
        uint32_t cp = big_endian ? ((uint32_t)src[i] << 8) | src[i + 1]
                                 : ((uint32_t)src[i + 1] << 8) | src[i];
        i += 2;
        if (!cp) break;
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len) {  /* surrogate pair */
            uint32_t lo = big_endian ? ((uint32_t)src[i] << 8) | src[i + 1]
                                     : ((uint32_t)src[i + 1] << 8) | src[i];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        if (cp < 0x80) {
            if (o + 1 >= cap) break;
            dst[o++] = (char)cp;
        } else if (cp < 0x800) {
            if (o + 2 >= cap) break;
            dst[o++] = (char)(0xC0 | (cp >> 6));
            dst[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            if (o + 3 >= cap) break;
            dst[o++] = (char)(0xE0 | (cp >> 12));
            dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            if (o + 4 >= cap) break;
            dst[o++] = (char)(0xF0 | (cp >> 18));
            dst[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[o++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    dst[o] = '\0';
}

/* Decode an ID3 text payload (first byte = encoding) into UTF-8. */
static void id3_text(char *dst, size_t cap, const uint8_t *d, size_t len)
{
    if (!len) { dst[0] = '\0'; return; }
    uint8_t enc = d[0];
    d++; len--;
    switch (enc) {
    case 0: latin1_to_utf8(dst, cap, d, len); break;
    case 1: utf16_to_utf8(dst, cap, d, len, false); break;
    case 2: utf16_to_utf8(dst, cap, d, len, true); break;
    case 3: default: {
        size_t n = len;
        while (n && d[n - 1] == 0) n--;
        if (n >= cap) n = cap - 1;
        memcpy(dst, d, n);
        dst[n] = '\0';
    } break;
    }
}

static uint32_t syncsafe32(const uint8_t *p)
{
    return ((uint32_t)(p[0] & 0x7F) << 21) | ((uint32_t)(p[1] & 0x7F) << 14) |
           ((uint32_t)(p[2] & 0x7F) << 7) | (p[3] & 0x7F);
}
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* ---------- APIC frame -> art location ---------------------------------- */

static void parse_apic(const uint8_t *d, uint32_t len, uint64_t frame_file_off,
                       track_tags_t *t)
{
    if (len < 4) return;
    uint8_t enc = d[0];
    uint32_t pos = 1;

    uint32_t mstart = pos;
    while (pos < len && d[pos]) pos++;
    if (pos >= len) return;
    uint32_t mlen = pos - mstart;
    pos++;                                   /* mime NUL */

    if (pos >= len) return;
    uint8_t ptype = d[pos++];                /* picture type */

    /* description: latin1/utf8 = 1 NUL; utf16 = 2-byte NUL */
    if (enc == 1 || enc == 2) {
        while (pos + 1 < len && (d[pos] || d[pos + 1])) pos += 2;
        pos += 2;
    } else {
        while (pos < len && d[pos]) pos++;
        pos++;
    }
    if (pos >= len) return;

    /* prefer front cover (3); else first */
    if (t->art_kind == ART_EMBEDDED && ptype != 3) return;

    t->art_kind = ART_EMBEDDED;
    t->art_offset = frame_file_off + pos;
    t->art_size = len - pos;
    uint32_t mc = mlen < TAG_MIME_MAX - 1 ? mlen : TAG_MIME_MAX - 1;
    memcpy(t->art_mime, d + mstart, mc);
    t->art_mime[mc] = '\0';
}

/* ---------- MPEG frame header probe: rate + duration --------------------- */

static const uint16_t br_v1_l3[16] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
static const uint16_t br_v2_l3[16] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};
static const uint16_t sr_tab[4] = {44100, 48000, 32000, 0};

static void mp3_probe_duration(pact_file_t *f, uint32_t audio_start, uint64_t file_size,
                               track_tags_t *t)
{
    uint8_t buf[192];
    pact_seek(f, audio_start);
    if (pact_read(f, buf, sizeof(buf)) < 16) return;

    /* find frame sync */
    uint32_t s = 0;
    while (s + 4 < sizeof(buf) && !(buf[s] == 0xFF && (buf[s + 1] & 0xE0) == 0xE0))
        s++;
    if (s + 4 >= sizeof(buf)) return;

    uint8_t b1 = buf[s + 1], b2 = buf[s + 2];
    int ver = (b1 >> 3) & 3;         /* 3=MPEG1, 2=MPEG2, 0=MPEG2.5 */
    int layer = (b1 >> 1) & 3;       /* 1 = Layer III */
    if (layer != 1) return;
    int brx = (b2 >> 4) & 0xF;
    int srx = (b2 >> 2) & 3;
    uint32_t rate = sr_tab[srx];
    if (!rate) return;
    if (ver == 2) rate /= 2;
    if (ver == 0) rate /= 4;
    uint32_t bitrate = (ver == 3 ? br_v1_l3[brx] : br_v2_l3[brx]) * 1000u;
    uint32_t spf = (ver == 3) ? 1152 : 576;
    t->sample_rate = rate;

    int ch_mode = (buf[s + 3] >> 6) & 3;    /* 3 = mono */
    uint32_t side = (ver == 3) ? (ch_mode == 3 ? 17 : 32)
                               : (ch_mode == 3 ? 9 : 17);
    uint32_t xoff = s + 4 + side;

    if (xoff + 16 < sizeof(buf) &&
        (memcmp(buf + xoff, "Xing", 4) == 0 || memcmp(buf + xoff, "Info", 4) == 0)) {
        uint32_t flags = be32(buf + xoff + 4);
        if (flags & 1) {
            uint32_t frames = be32(buf + xoff + 8);
            t->duration_ms = (uint32_t)((uint64_t)frames * spf * 1000 / rate);
            return;
        }
    }
    if (s + 4 + 32 + 14 + 4 < sizeof(buf) && memcmp(buf + s + 4 + 32, "VBRI", 4) == 0) {
        uint32_t frames = be32(buf + s + 4 + 32 + 14);
        t->duration_ms = (uint32_t)((uint64_t)frames * spf * 1000 / rate);
        return;
    }
    if (bitrate)  /* CBR estimate */
        t->duration_ms = (uint32_t)((file_size - audio_start) * 8000ull / bitrate);
}

/* ---------- ID3v1 fallback ---------------------------------------------- */

static void id3v1_read(pact_file_t *f, uint64_t file_size, track_tags_t *t)
{
    if (file_size < 128) return;
    uint8_t v1[128];
    pact_seek(f, file_size - 128);
    if (pact_read(f, v1, 128) != 128 || memcmp(v1, "TAG", 3) != 0) return;

    if (!t->title[0])  latin1_to_utf8(t->title,  TAG_STR_MAX, v1 + 3, 30);
    if (!t->artist[0]) latin1_to_utf8(t->artist, TAG_STR_MAX, v1 + 33, 30);
    if (!t->album[0])  latin1_to_utf8(t->album,  TAG_STR_MAX, v1 + 63, 30);
    if (!t->track_no && v1[125] == 0 && v1[126]) t->track_no = v1[126];
}

/* ---------- main entry ---------------------------------------------------*/

bool tags_read_mp3(const char *path, track_tags_t *t)
{
    pact_file_t *f = pact_open(path);
    if (!f) return false;
    uint64_t file_size = pact_size(f);

    uint32_t audio_start = 0;
    uint8_t h[10];
    if (pact_read(f, h, 10) == 10 && memcmp(h, "ID3", 3) == 0) {
        uint8_t ver = h[3];
        bool global_unsync = h[5] & 0x80;
        uint32_t tag_size = syncsafe32(h + 6);
        audio_start = 10 + tag_size;

        uint32_t pos = 10;
        if (h[5] & 0x40) {  /* extended header: skip */
            uint8_t eh[4];
            if (pact_read(f, eh, 4) == 4) {
                uint32_t esz = (ver == 4) ? syncsafe32(eh) : be32(eh);
                if (ver == 3) esz += 4;
                pact_seek(f, pos + esz);
                pos += esz + ((ver == 4) ? 0 : 0);
                pos += 4;
            }
        }

        while (pos + 10 < 10 + tag_size) {
            uint8_t fh[10];
            pact_seek(f, pos);
            if (pact_read(f, fh, 10) != 10) break;
            if (!fh[0]) break;  /* padding */
            uint32_t fsize = (ver == 4) ? syncsafe32(fh + 4) : be32(fh + 4);
            if (!fsize || pos + 10 + fsize > 10 + tag_size) break;
            bool frame_unsync = (ver == 4) && (fh[9] & 0x02);

            bool want_text = !memcmp(fh, "TIT2", 4) || !memcmp(fh, "TPE1", 4) ||
                             !memcmp(fh, "TALB", 4) || !memcmp(fh, "TRCK", 4);
            bool want_apic = !memcmp(fh, "APIC", 4);

            if ((want_text || want_apic) && fsize <= (16u << 20)) {
                uint8_t *d = malloc(fsize);
                if (d && pact_read(f, d, fsize) == fsize) {
                    uint32_t dlen = fsize;
                    if (global_unsync || frame_unsync) {
                        /* remove 0xFF 0x00 stuffing in place */
                        uint32_t o = 0;
                        for (uint32_t i = 0; i < fsize; i++) {
                            d[o++] = d[i];
                            if (d[i] == 0xFF && i + 1 < fsize && d[i + 1] == 0x00)
                                i++;
                        }
                        dlen = o;
                        /* offsets into the file are no longer valid for art */
                        if (want_apic) { free(d); pos += 10 + fsize; continue; }
                    }
                    if (want_apic) {
                        parse_apic(d, dlen, pos + 10, t);
                    } else if (!memcmp(fh, "TRCK", 4)) {
                        char num[16];
                        id3_text(num, sizeof(num), d, dlen);
                        t->track_no = (uint16_t)atoi(num);
                    } else {
                        char *dst = !memcmp(fh, "TIT2", 4) ? t->title :
                                    !memcmp(fh, "TPE1", 4) ? t->artist : t->album;
                        id3_text(dst, TAG_STR_MAX, d, dlen);
                    }
                }
                free(d);
            }
            pos += 10 + fsize;
        }
    }

    if (!t->title[0] || !t->artist[0] || !t->album[0])
        id3v1_read(f, file_size, t);

    mp3_probe_duration(f, audio_start, file_size, t);
    t->bits_per_sample = 0;

    pact_close(f);
    return true;
}
