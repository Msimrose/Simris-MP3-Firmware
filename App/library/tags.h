/*
 * Pact MP-1 - track metadata: small custom parsers, exactly what the UI
 * needs (title / artist / album / track no / duration / format badge / art
 * location) and nothing else. FLAC = Vorbis comments + PICTURE block,
 * MP3 = ID3v2.3/2.4 (+ID3v1 fallback) + Xing/VBRI duration probe.
 *
 * Art is never copied at scan time: we record where the image bytes live
 * (embedded offset+size, or a folder image path) and extract on demand.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TAG_STR_MAX  128
#define TAG_MIME_MAX 32

typedef enum {
    ART_NONE = 0,
    ART_EMBEDDED,     /* image bytes inside the audio file itself */
    ART_FOLDER_FILE,  /* cover.jpg / folder.jpg next to the track */
} art_kind_t;

typedef struct {
    char     title[TAG_STR_MAX];   /* UTF-8; falls back to filename */
    char     artist[TAG_STR_MAX];
    char     album[TAG_STR_MAX];
    uint16_t track_no;
    uint32_t duration_ms;
    uint32_t sample_rate;
    uint8_t  bits_per_sample;      /* 0 = lossy */

    art_kind_t art_kind;
    uint64_t art_offset;           /* ART_EMBEDDED only */
    uint32_t art_size;
    char     art_mime[TAG_MIME_MAX];
} track_tags_t;

bool tags_read_flac(const char *path, track_tags_t *out);
bool tags_read_mp3(const char *path, track_tags_t *out);

/* Dispatch by extension; WAV gets header info + filename title. Always
 * leaves *out in a usable state (filename fallback) if the file opens. */
bool tags_read(const char *path, track_tags_t *out);
