/*
 * Pact MP-1 - decoder abstraction: one interface over FLAC / MP3 / WAV.
 *
 * Output is always interleaved stereo int32, left-justified full scale
 * (16-bit sources sit in the top 16 bits, 24-bit in the top 24). Mono
 * sources are duplicated to both channels. No resampling ever happens
 * here: the caller switches the output clock to fmt.sample_rate.
 *
 * File IO currently goes through each library's stdio path (host build).
 * When FatFs lands, the swap to IO callbacks is contained inside the three
 * decoder_*.c files; this interface does not change.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t sample_rate;
    uint8_t  channels;         /* source channel count (output is always 2) */
    uint8_t  bits_per_sample;  /* source depth, informational (MP3: 0) */
    uint64_t total_frames;     /* 0 if unknown */
} audio_fmt_t;

typedef struct audio_source audio_source_t;

struct audio_source {
    audio_fmt_t fmt;
    /* read up to n stereo frames into out (2*n int32); returns frames read */
    size_t (*read)(audio_source_t *s, int32_t *out, size_t n);
    bool   (*seek)(audio_source_t *s, uint64_t frame);
    void   (*close)(audio_source_t *s);
    void  *impl;
};

/* Dispatches on file extension (.flac / .mp3 / .wav). NULL on failure. */
audio_source_t *audio_source_open(const char *path);
void            audio_source_close(audio_source_t *s);

/* Backends (used by the dispatcher) */
audio_source_t *audio_source_open_flac(const char *path);
audio_source_t *audio_source_open_mp3(const char *path);
audio_source_t *audio_source_open_wav(const char *path);
