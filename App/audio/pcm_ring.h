/*
 * Pact MP-1 - lock-free single-producer single-consumer PCM ring.
 *
 * Frames are interleaved stereo int32 (left-justified full-scale, the SAI
 * wire format). Producer = decoder pump (audio task / host thread).
 * Consumer = SAI DMA half/full callback on device, SDL audio callback on host.
 * Capacity must be a power of two.
 *
 * Device placement rule (firmware-spec section 6): the storage buffer must
 * live in D2 SRAM or AXI, MPU non-cacheable, never DTCM.
 */
#pragma once
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t      *buf;       /* interleaved L,R pairs; capacity*2 int32 */
    size_t        capacity;  /* frames, power of two */
    atomic_size_t head;      /* write position (frames, free-running) */
    atomic_size_t tail;      /* read position (frames, free-running) */
} pcm_ring_t;

void   pcm_ring_init(pcm_ring_t *r, int32_t *storage, size_t capacity_frames);
void   pcm_ring_clear(pcm_ring_t *r);
size_t pcm_ring_readable(const pcm_ring_t *r);  /* frames available to read */
size_t pcm_ring_writable(const pcm_ring_t *r);  /* frames of free space */
size_t pcm_ring_write(pcm_ring_t *r, const int32_t *frames, size_t n);
size_t pcm_ring_read(pcm_ring_t *r, int32_t *out, size_t n);
