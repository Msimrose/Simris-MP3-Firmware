#include "pcm_ring.h"
#include <string.h>

void pcm_ring_init(pcm_ring_t *r, int32_t *storage, size_t capacity_frames)
{
    r->buf = storage;
    r->capacity = capacity_frames;
    atomic_store(&r->head, 0);
    atomic_store(&r->tail, 0);
}

void pcm_ring_clear(pcm_ring_t *r)
{
    /* consumer must be silent/stopped when clearing */
    atomic_store(&r->tail, atomic_load(&r->head));
}

size_t pcm_ring_readable(const pcm_ring_t *r)
{
    size_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    size_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    return h - t;
}

size_t pcm_ring_writable(const pcm_ring_t *r)
{
    return r->capacity - pcm_ring_readable(r);
}

size_t pcm_ring_write(pcm_ring_t *r, const int32_t *frames, size_t n)
{
    size_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    size_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    size_t free = r->capacity - (h - t);
    if (n > free) n = free;

    size_t idx = h & (r->capacity - 1);
    size_t first = r->capacity - idx;      /* frames until wrap */
    if (first > n) first = n;

    memcpy(&r->buf[idx * 2], frames, first * 2 * sizeof(int32_t));
    if (n > first)
        memcpy(&r->buf[0], frames + first * 2, (n - first) * 2 * sizeof(int32_t));

    atomic_store_explicit(&r->head, h + n, memory_order_release);
    return n;
}

size_t pcm_ring_read(pcm_ring_t *r, int32_t *out, size_t n)
{
    size_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    size_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    size_t avail = h - t;
    if (n > avail) n = avail;

    size_t idx = t & (r->capacity - 1);
    size_t first = r->capacity - idx;
    if (first > n) first = n;

    memcpy(out, &r->buf[idx * 2], first * 2 * sizeof(int32_t));
    if (n > first)
        memcpy(out + first * 2, &r->buf[0], (n - first) * 2 * sizeof(int32_t));

    atomic_store_explicit(&r->tail, t + n, memory_order_release);
    return n;
}
