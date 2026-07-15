/*
 * Pact MP-1 - audio engine: track lifecycle + the decode pump.
 *
 * Threading model (same on device and host):
 *   - control calls (play/pause/seek/volume) come from the UI thread
 *   - audio_engine_pump() runs on the audio thread (FreeRTOS audio_task /
 *     host pthread) and fills the PCM ring ahead of the consumer
 *   - the output driver (SAI DMA / SDL callback) only ever reads the ring
 */
#pragma once
#include "audio_source.h"
#include "pcm_ring.h"

typedef enum {
    ENGINE_STOPPED = 0,
    ENGINE_PLAYING,
    ENGINE_PAUSED,
    ENGINE_FINISHED,   /* track ended; ring may still be draining */
} engine_state_t;

void            audio_engine_init(pcm_ring_t *ring);
bool            audio_engine_play(const char *path);      /* open + start */

/* Gapless: queue the track to take over the ring when the current one is
 * fully decoded - same sample-rate family only (a rate change needs the
 * output clock switched, so it falls back to ENGINE_FINISHED and the
 * caller's normal advance path). NULL clears. play()/stop() also clear. */
void            audio_engine_set_next(const char *path);

/* Increments on every source change (play() and gapless takeovers).
 * Poll it to notice engine-initiated advances; note it bumps when the
 * DECODER crosses the boundary - playback is up to a ring-depth behind. */
uint32_t        audio_engine_track_serial(void);
void            audio_engine_pause(void);
void            audio_engine_resume(void);
void            audio_engine_stop(void);
bool            audio_engine_seek(double seconds);
void            audio_engine_set_volume(int step);        /* 0..31 */
int             audio_engine_volume(void);
engine_state_t  audio_engine_state(void);
const audio_fmt_t *audio_engine_fmt(void);                /* NULL if stopped */
double          audio_engine_position(void);              /* seconds decoded */

/* Run one pump iteration; returns true if it did work (caller may sleep
 * briefly when it returns false). */
bool audio_engine_pump(void);
