/* FLAC backend: dr_flac over pact_io callbacks, s32 output. */
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG
#define DR_FLAC_NO_STDIO
#include "dr_libs/dr_flac.h"

#include "audio_source.h"
#include "../pact_io.h"
#include <stdlib.h>

#define MONO_CHUNK 1024

typedef struct {
    drflac      *f;
    pact_file_t *io;
    int32_t      mono[MONO_CHUNK];
} flac_impl_t;

static size_t flac_read_cb(void *ud, void *out, size_t bytes)
{
    return pact_read((pact_file_t *)ud, out, bytes);
}

static drflac_bool32 flac_seek_cb(void *ud, int offset, drflac_seek_origin origin)
{
    pact_file_t *io = ud;
    uint64_t base = 0;
    if (origin == DRFLAC_SEEK_CUR) base = pact_tell(io);
    else if (origin == DRFLAC_SEEK_END) base = pact_size(io);
    return pact_seek(io, base + (uint64_t)(int64_t)offset) ? DRFLAC_TRUE
                                                           : DRFLAC_FALSE;
}

static drflac_bool32 flac_tell_cb(void *ud, drflac_int64 *cursor)
{
    *cursor = (drflac_int64)pact_tell((pact_file_t *)ud);
    return DRFLAC_TRUE;
}

static size_t flac_read(audio_source_t *s, int32_t *out, size_t n)
{
    flac_impl_t *im = s->impl;
    if (im->f->channels >= 2)
        return (size_t)drflac_read_pcm_frames_s32(im->f, n, (drflac_int32 *)out);

    size_t done = 0;
    while (done < n) {
        size_t want = n - done;
        if (want > MONO_CHUNK) want = MONO_CHUNK;
        size_t got = (size_t)drflac_read_pcm_frames_s32(im->f, want,
                                                        (drflac_int32 *)im->mono);
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
    pact_close(im->io);
    free(im);
    free(s);
}

audio_source_t *audio_source_open_flac(const char *path)
{
    pact_file_t *io = pact_open(path);
    if (!io) return NULL;

    drflac *f = drflac_open(flac_read_cb, flac_seek_cb, flac_tell_cb, io, NULL);
    if (!f) {
        pact_close(io);
        return NULL;
    }

    audio_source_t *s = calloc(1, sizeof(*s));
    flac_impl_t *im = calloc(1, sizeof(*im));
    im->f = f;
    im->io = io;

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
