/* FLAC backend: dr_flac, s32 output (16/24-bit sources left-justified). */
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG
#include "dr_libs/dr_flac.h"

#include "audio_source.h"
#include <stdlib.h>

#define MONO_CHUNK 1024

typedef struct {
    drflac  *f;
    int32_t  mono[MONO_CHUNK];
} flac_impl_t;

static size_t flac_read(audio_source_t *s, int32_t *out, size_t n)
{
    flac_impl_t *im = s->impl;
    if (im->f->channels >= 2) {
        /* dr_flac interleaves; for >2ch we would need channel folding, but
         * the dispatcher only sees mono/stereo in practice on this device */
        return (size_t)drflac_read_pcm_frames_s32(im->f, n, out);
    }
    /* mono: decode then duplicate */
    size_t done = 0;
    while (done < n) {
        size_t want = n - done;
        if (want > MONO_CHUNK) want = MONO_CHUNK;
        size_t got = (size_t)drflac_read_pcm_frames_s32(im->f, want, im->mono);
        for (size_t i = 0; i < got; i++) {
            out[(done + i) * 2 + 0] = im->mono[i];
            out[(done + i) * 2 + 1] = im->mono[i];
        }
        done += got;
        if (got < want) break;
    }
    return done;
}

static bool flac_seek(audio_source_t *s, uint64_t frame)
{
    flac_impl_t *im = s->impl;
    return drflac_seek_to_pcm_frame(im->f, frame) == DRFLAC_TRUE;
}

static void flac_close(audio_source_t *s)
{
    flac_impl_t *im = s->impl;
    drflac_close(im->f);
    free(im);
    free(s);
}

audio_source_t *audio_source_open_flac(const char *path)
{
    drflac *f = drflac_open_file(path, NULL);
    if (!f) return NULL;

    audio_source_t *s = calloc(1, sizeof(*s));
    flac_impl_t *im = calloc(1, sizeof(*im));
    im->f = f;

    s->impl = im;
    s->fmt.sample_rate = f->sampleRate;
    s->fmt.channels = f->channels;
    s->fmt.bits_per_sample = f->bitsPerSample;
    s->fmt.total_frames = f->totalPCMFrameCount;
    s->read = flac_read;
    s->seek = flac_seek;
    s->close = flac_close;
    return s;
}
