#include "audio_engine.h"
#include "audio_port.h"
#include "volume.h"
#include <string.h>

#define PUMP_CHUNK 1024  /* frames per pump iteration */

static struct {
    pcm_ring_t     *ring;
    audio_source_t *src;
    engine_state_t  state;
    int             vol_step;
    uint64_t        frames_out;   /* frames decoded since track start/seek */
    uint64_t        seek_base;    /* frame position the count restarts from */
    pact_mutex_t    lock;
    int32_t         chunk[PUMP_CHUNK * 2];
} eng = { .vol_step = VOLUME_STEPS - 1 };

void audio_engine_init(pcm_ring_t *ring)
{
    eng.ring = ring;
    eng.state = ENGINE_STOPPED;
    pact_mutex_init(&eng.lock);
}

static void close_src_locked(void)
{
    if (eng.src) {
        audio_source_close(eng.src);
        eng.src = NULL;
    }
}

bool audio_engine_play(const char *path)
{
    pact_mutex_lock(&eng.lock);
    close_src_locked();
    pcm_ring_clear(eng.ring);
    eng.src = audio_source_open(path);
    eng.frames_out = 0;
    eng.seek_base = 0;
    eng.state = eng.src ? ENGINE_PLAYING : ENGINE_STOPPED;
    pact_mutex_unlock(&eng.lock);
    return eng.src != NULL;
}

void audio_engine_pause(void)
{
    pact_mutex_lock(&eng.lock);
    if (eng.state == ENGINE_PLAYING) eng.state = ENGINE_PAUSED;
    pact_mutex_unlock(&eng.lock);
}

void audio_engine_resume(void)
{
    pact_mutex_lock(&eng.lock);
    if (eng.state == ENGINE_PAUSED) eng.state = ENGINE_PLAYING;
    pact_mutex_unlock(&eng.lock);
}

void audio_engine_stop(void)
{
    pact_mutex_lock(&eng.lock);
    close_src_locked();
    pcm_ring_clear(eng.ring);
    eng.state = ENGINE_STOPPED;
    pact_mutex_unlock(&eng.lock);
}

bool audio_engine_seek(double seconds)
{
    bool ok = false;
    pact_mutex_lock(&eng.lock);
    if (eng.src) {
        uint64_t frame = (uint64_t)(seconds * eng.src->fmt.sample_rate);
        if (eng.src->fmt.total_frames && frame >= eng.src->fmt.total_frames)
            frame = eng.src->fmt.total_frames ? eng.src->fmt.total_frames - 1 : 0;
        ok = eng.src->seek(eng.src, frame);
        if (ok) {
            pcm_ring_clear(eng.ring);
            eng.seek_base = frame;
            eng.frames_out = 0;
            if (eng.state == ENGINE_FINISHED) eng.state = ENGINE_PLAYING;
        }
    }
    pact_mutex_unlock(&eng.lock);
    return ok;
}

void audio_engine_set_volume(int step)
{
    if (step < 0) step = 0;
    if (step > VOLUME_STEPS - 1) step = VOLUME_STEPS - 1;
    eng.vol_step = step;
}

int audio_engine_volume(void) { return eng.vol_step; }

engine_state_t audio_engine_state(void) { return eng.state; }

const audio_fmt_t *audio_engine_fmt(void)
{
    return eng.src ? &eng.src->fmt : NULL;
}

double audio_engine_position(void)
{
    if (!eng.src) return 0.0;
    return (double)(eng.seek_base + eng.frames_out) / eng.src->fmt.sample_rate;
}

bool audio_engine_pump(void)
{
    pact_mutex_lock(&eng.lock);
    if (eng.state != ENGINE_PLAYING || !eng.src ||
        pcm_ring_writable(eng.ring) < PUMP_CHUNK) {
        pact_mutex_unlock(&eng.lock);
        return false;
    }

    size_t got = eng.src->read(eng.src, eng.chunk, PUMP_CHUNK);
    if (got > 0) {
        volume_apply(eng.chunk, got * 2, eng.vol_step);
        pcm_ring_write(eng.ring, eng.chunk, got);
        eng.frames_out += got;
    }
    if (got < PUMP_CHUNK)
        eng.state = ENGINE_FINISHED;

    pact_mutex_unlock(&eng.lock);
    return got > 0;
}
