/* WAV backend: dr_wav, s32 output. */
#define DR_WAV_IMPLEMENTATION
#include "dr_libs/dr_wav.h"

#include "audio_source.h"
#include <stdlib.h>

#define WAV_MONO_CHUNK 1024

typedef struct {
    drwav   w;
    int32_t mono[WAV_MONO_CHUNK];
} wav_impl_t;

static size_t wav_read(audio_source_t *s, int32_t *out, size_t n)
{
    wav_impl_t *im = s->impl;
    if (im->w.channels >= 2)
        return (size_t)drwav_read_pcm_frames_s32(&im->w, n, (drwav_int32 *)out);

    size_t done = 0;
    while (done < n) {
        size_t want = n - done;
        if (want > WAV_MONO_CHUNK) want = WAV_MONO_CHUNK;
        size_t got = (size_t)drwav_read_pcm_frames_s32(&im->w, want, (drwav_int32 *)im->mono);
        for (size_t i = 0; i < got; i++) {
            out[(done + i) * 2 + 0] = im->mono[i];
            out[(done + i) * 2 + 1] = im->mono[i];
        }
        done += got;
        if (got < want) break;
    }
    return done;
}

static bool wav_seek(audio_source_t *s, uint64_t frame)
{
    wav_impl_t *im = s->impl;
    return drwav_seek_to_pcm_frame(&im->w, frame) == DRWAV_TRUE;
}

static void wav_close(audio_source_t *s)
{
    wav_impl_t *im = s->impl;
    drwav_uninit(&im->w);
    free(im);
    free(s);
}

audio_source_t *audio_source_open_wav(const char *path)
{
    wav_impl_t *im = calloc(1, sizeof(*im));
    if (!drwav_init_file(&im->w, path, NULL)) {
        free(im);
        return NULL;
    }

    audio_source_t *s = calloc(1, sizeof(*s));
    s->impl = im;
    s->fmt.sample_rate = im->w.sampleRate;
    s->fmt.channels = im->w.channels;
    s->fmt.bits_per_sample = im->w.bitsPerSample;
    s->fmt.total_frames = im->w.totalPCMFrameCount;
    s->read = wav_read;
    s->seek = wav_seek;
    s->close = wav_close;
    return s;
}
