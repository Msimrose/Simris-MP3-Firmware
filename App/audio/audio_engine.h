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
