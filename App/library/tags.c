#include "tags.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Minimal RIFF/WAVE header probe: rate, bits, duration. No tags in WAVs. */
static bool wav_probe(const char *path, track_tags_t *t)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
        fclose(f);
        return false;
    }

    uint32_t rate = 0, byterate = 0, datalen = 0;
    uint16_t bits = 0;
    uint8_t ch[8];
    for (int guard = 0; guard < 64 && fread(ch, 1, 8, f) == 8; guard++) {
        uint32_t clen = (uint32_t)ch[4] | ((uint32_t)ch[5] << 8) |
                        ((uint32_t)ch[6] << 16) | ((uint32_t)ch[7] << 24);
        if (!memcmp(ch, "fmt ", 4) && clen >= 16) {
            uint8_t fmt[16];
            if (fread(fmt, 1, 16, f) != 16) break;
            rate = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) |
                   ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
            byterate = (uint32_t)fmt[8] | ((uint32_t)fmt[9] << 8) |
                       ((uint32_t)fmt[10] << 16) | ((uint32_t)fmt[11] << 24);
            bits = (uint16_t)(fmt[14] | (fmt[15] << 8));
            if (clen > 16) fseek(f, (long)(clen - 16), SEEK_CUR);
        } else if (!memcmp(ch, "data", 4)) {
            datalen = clen;
            break;
        } else {
            fseek(f, (long)(clen + (clen & 1)), SEEK_CUR);
        }
    }
    fclose(f);

    t->sample_rate = rate;
    t->bits_per_sample = (uint8_t)bits;
    if (byterate) t->duration_ms = (uint32_t)((uint64_t)datalen * 1000 / byterate);
    return true;
}

static void filename_fallback(const char *path, track_tags_t *t)
{
    if (t->title[0]) return;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    size_t n = dot ? (size_t)(dot - base) : strlen(base);
    if (n >= TAG_STR_MAX) n = TAG_STR_MAX - 1;
    memcpy(t->title, base, n);
    t->title[n] = '\0';
}

bool tags_read(const char *path, track_tags_t *t)
{
    memset(t, 0, sizeof(*t));

    const char *dot = strrchr(path, '.');
    const char *ext = dot ? dot + 1 : "";
    bool ok = false;

    if (strcasecmp(ext, "flac") == 0)     ok = tags_read_flac(path, t);
    else if (strcasecmp(ext, "mp3") == 0) ok = tags_read_mp3(path, t);
    else if (strcasecmp(ext, "wav") == 0) ok = wav_probe(path, t);

    if (ok) filename_fallback(path, t);
    return ok;
}
