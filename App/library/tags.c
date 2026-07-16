#include "tags.h"
#include "../pact_io.h"
#include <string.h>
#include <strings.h>

/* Minimal RIFF/WAVE header probe: rate, bits, duration. No tags in WAVs. */
static bool wav_probe(const char *path, track_tags_t *t)
{
    pact_file_t *f = pact_open(path);
    if (!f) return false;

    uint8_t h[12];
    if (pact_read(f, h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
        pact_close(f);
        return false;
    }

    uint32_t rate = 0, byterate = 0, datalen = 0;
    uint16_t bits = 0;
    uint8_t ch[8];
    for (int guard = 0; guard < 64 && pact_read(f, ch, 8) == 8; guard++) {
        uint32_t clen = (uint32_t)ch[4] | ((uint32_t)ch[5] << 8) |
                        ((uint32_t)ch[6] << 16) | ((uint32_t)ch[7] << 24);
        if (!memcmp(ch, "fmt ", 4) && clen >= 16) {
            uint8_t fmt[16];
            if (pact_read(f, fmt, 16) != 16) break;
            rate = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) |
                   ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
            byterate = (uint32_t)fmt[8] | ((uint32_t)fmt[9] << 8) |
                       ((uint32_t)fmt[10] << 16) | ((uint32_t)fmt[11] << 24);
            bits = (uint16_t)(fmt[14] | (fmt[15] << 8));
            if (clen > 16) pact_seek(f, pact_tell(f) + (clen - 16));
        } else if (!memcmp(ch, "data", 4)) {
            datalen = clen;
            break;
        } else {
            pact_seek(f, pact_tell(f) + clen + (clen & 1));
        }
    }
    pact_close(f);

    t->sample_rate = rate;
    t->bits_per_sample = (uint8_t)bits;
    if (byterate) t->duration_ms = (uint32_t)((uint64_t)datalen * 1000 / byterate);
    return true;
}

/* Untagged files (WAVs mostly - field recordings, bounces): title from
 * the filename stem, album from the parent folder so a folder of WAVs
 * browses like an album. */
static void filename_fallback(const char *path, track_tags_t *t)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    if (!t->title[0]) {
        const char *dot = strrchr(base, '.');
        size_t n = dot ? (size_t)(dot - base) : strlen(base);

        /* rip convention "NN. Artist - Title": number -> track_no,
         * left of " - " -> artist, right -> title */
        size_t i = 0;
        unsigned no = 0;
        while (i < n && base[i] >= '0' && base[i] <= '9')
            no = no * 10 + (unsigned)(base[i++] - '0');
        if (i > 0 && i < n && (base[i] == '.' || base[i] == '-' ||
                               base[i] == '_' || base[i] == ' ')) {
            while (i < n && (base[i] == '.' || base[i] == '-' ||
                             base[i] == '_' || base[i] == ' '))
                i++;
            if (!t->track_no && no) t->track_no = (uint16_t)no;
        } else {
            i = 0;                        /* no leading number */
        }

        const char *rest = base + i;
        size_t rn = n - i;
        const char *sep = NULL;
        for (size_t k = 0; k + 3 <= rn; k++)
            if (rest[k] == ' ' && rest[k + 1] == '-' && rest[k + 2] == ' ') {
                sep = rest + k;
                break;
            }
        if (sep && !t->artist[0] && sep > rest) {
            size_t an = (size_t)(sep - rest);
            if (an >= TAG_STR_MAX) an = TAG_STR_MAX - 1;
            memcpy(t->artist, rest, an);
            t->artist[an] = '\0';
            rest = sep + 3;
            rn = n - i - an - 3;
        }
        if (rn >= TAG_STR_MAX) rn = TAG_STR_MAX - 1;
        memcpy(t->title, rest, rn);
        t->title[rn] = '\0';
    }

    if (!t->album[0] && base > path) {
        const char *dir_end = base - 1;              /* the '/' */
        const char *dir = dir_end;
        while (dir > path && dir[-1] != '/') dir--;
        size_t n = (size_t)(dir_end - dir);
        /* skip volume roots like "1:" */
        if (n && !(n == 2 && dir[1] == ':')) {
            if (n >= TAG_STR_MAX) n = TAG_STR_MAX - 1;
            memcpy(t->album, dir, n);
            t->album[n] = '\0';
        }
    }
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
