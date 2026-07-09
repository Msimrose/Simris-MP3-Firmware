/*
 * Pact MP-1 - digital volume: 32 steps, log taper (2 dB/step), Q31 gain.
 *
 * Step 31 is a true bit-perfect bypass (no multiply at all). Step 0 is mute.
 * Processing is a 64-bit multiply with round-to-nearest, then saturation,
 * on full-scale int32 samples. At 24-bit DAC depth the rounding error floor
 * sits far below audibility; no dither is applied (revisit in Phase 8 if
 * measurements ever argue for it).
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#define VOLUME_STEPS 32   /* steps 0..31 */

/* Apply volume in place to n stereo frames (2*n samples). */
void volume_apply(int32_t *samples, size_t n_samples, int step);
