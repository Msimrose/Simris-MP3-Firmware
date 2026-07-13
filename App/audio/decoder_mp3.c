/*
 * MP3 backend: minimp3_ex over pact_io callbacks, float output for maximum
 * decode precision, converted once to full-scale int32 round-to-nearest.
 *
 * mp3dec_ex handles Xing/LAME tags including encoder delay/padding trim,
 * which is what makes gapless playback possible later.
 */
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
#include "minimp3/minimp3_ex.h"

#include "audio_source.h"
#include "../pact_io.h"
#include <math.h>
#include <stdlib.h>

#define MP3_CHUNK 1152  /* one MPEG frame of samples per channel */

typedef struct {
    mp3dec_ex_t  dec;
    mp3dec_io_t  cbio;
    pact_file_t *io;
    float        fbuf[MP3_CHUNK * 2];
} mp3_impl_t;

static size_t mp3_read_cb(void *buf, size_t size, void *ud)
{
    return pact_read((pact_file_t *)ud, buf, size);
}

static int mp3_seek_cb(uint64_t position, void *ud)
{
    return pact_seek((pact_file_t *)ud, position) ? 0 : -1;
}

static inline int32_t f_to_s32(float x)
{
    float v = x * 2147483648.0f;
    if (v >=  2147483647.0f) return INT32_MAX;
    if (v <= -2147483648.0f) return INT32_MIN;
    return (int32_t)lrintf(v);
}

static size_t mp3_read(audio_source_t *s, int32_t *out, size_t n)
{
    mp3_impl_t *im = s->impl;
    unsigned ch = im->dec.info.channels;
    size_t done = 0;

    while (done < n) {
        size_t want_frames = n - done;
        if (want_frames > MP3_CHUNK) want_frames = MP3_CHUNK;
        size_t got_samples = mp3dec_ex_read(&im->dec, im->fbuf, want_frames * ch);
        size_t got_frames = got_samples / ch;
        if (ch >= 2) {
            for (size_t i = 0; i < got_frames; i++) {
                out[(done + i) * 2 + 0] = f_to_s32(im->fbuf[i * ch + 0]);
                out[(done + i) * 2 + 1] = f_to_s32(im->fbuf[i * ch + 1]);
            }
        } else {
            for (size_t i = 0; i < got_frames; i++) {
                int32_t v = f_to_s32(im->fbuf[i]);
                out[(done + i) * 2 + 0] = v;
                out[(done + i) * 2 + 1] = v;
            }
        }
        done += got_frames;
        if (got_frames < want_frames) break;  /* end of stream */
    }
    return done;
}

static bool mp3_seek(audio_source_t *s, uint64_t frame)
{
    mp3_impl_t *im = s->impl;
    return mp3dec_ex_seek(&im->dec, frame * im->dec.info.channels) == 0;
}

static void mp3_close(audio_source_t *s)
{
    mp3_impl_t *im = s->impl;
    mp3dec_ex_close(&im->dec);
    pact_close(im->io);
    free(im);
    free(s);
}

audio_source_t *audio_source_open_mp3(const char *path)
{
    pact_file_t *io = pact_open(path);
    if (!io) return NULL;

    mp3_impl_t *im = calloc(1, sizeof(*im));
    im->io = io;
    im->cbio.read = mp3_read_cb;
    im->cbio.read_data = io;
    im->cbio.seek = mp3_seek_cb;
    im->cbio.seek_data = io;

    if (mp3dec_ex_open_cb(&im->dec, &im->cbio, MP3D_SEEK_TO_SAMPLE) != 0 ||
        im->dec.info.channels == 0) {
        mp3dec_ex_close(&im->dec);
        pact_close(io);
        free(im);
        return NULL;
    }

    audio_source_t *s = calloc(1, sizeof(*s));
    s->impl = im;
    s->fmt.sample_rate = im->dec.info.hz;
    s->fmt.channels = im->dec.info.channels;
    s->fmt.bits_per_sample = 0; /* lossy: no native depth */
    s->fmt.total_frames = im->dec.samples / im->dec.info.channels;
    s->read = mp3_read;
    s->seek = mp3_seek;
    s->close = mp3_close;
    return s;
}
