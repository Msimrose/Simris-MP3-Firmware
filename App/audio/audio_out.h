/*
 * Pact MP-1 - audio output driver interface.
 *
 * Device implementation: App/hal/sai_out.c (SAI1 + circular DMA feeding the
 * PCM5102A, pop-free XSMT/AMP power sequencing, PLL2/PLL3 kernel mux per
 * track sample-rate family). The Mac simulator uses SDL directly (sim/main.c)
 * and does not implement this interface.
 *
 * Contract: the driver is the single consumer of the PCM ring the engine
 * fills. Control calls (init/start/stop) come from the player/audio task,
 * never from ISRs. The wakeup hook IS called from ISR context after each
 * DMA half is consumed - keep it to a task notification.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "pcm_ring.h"

void     audio_out_init(pcm_ring_t *ring);

/* Full pop-free start at the given rate. Safe to call while running: same
 * rate is a no-op, a new rate does a muted reconfigure (rails stay up).
 * Returns false for rates neither PLL family can hit exactly. */
bool     audio_out_start(uint32_t sample_rate);

/* Reverse sequence: amp off, soft-mute, DMA stop, +/-5V rails down. */
void     audio_out_stop(void);

bool     audio_out_running(void);
uint32_t audio_out_sample_rate(void);   /* 0 when stopped */

/* ISR-context hook, fired after every consumed DMA half; wire it to the
 * audio task notification so the decoder pump wakes to refill the ring. */
void     audio_out_set_wakeup(void (*hook)(void));

/* Halves that came up short and were zero-padded. Benign while paused or
 * draining at track end; climbing during playback means the pump is late. */
uint32_t audio_out_underruns(void);
uint32_t audio_out_errors(void);        /* HAL_SAI_ErrorCallback count */
