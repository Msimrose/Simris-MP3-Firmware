/*
 * Pact MP-1 - newlib heap in AXI SRAM.
 *
 * The default heap (Core/Src/sysmem.c) grows between .bss and the MSP
 * stack in DTCM - barely 40 KB once the RTOS heap is placed, and decoder
 * opens (dr_flac block caches run ~35 KB for 4096-sample streams) plus the
 * library's realloc'd index arrays outgrow it immediately. The linker
 * redirects _sbrk here (CMakeLists: -Wl,--wrap=_sbrk) so newlib's malloc
 * draws from a dedicated AXI arena instead.
 *
 * Placement rules stay intact: DTCM keeps the FreeRTOS heap + task stacks
 * (fast, no DMA), and nothing DMA-driven may ever live on the malloc heap -
 * DMA buffers are explicit PACT_D2 statics (non-cacheable MPU region).
 *
 * nano-malloc is not thread-safe by itself; the weak newlib lock hooks get
 * real implementations below (scheduler suspension). malloc is legal from
 * any task, never from an ISR.
 */
#ifndef PACT_SIM

#include "FreeRTOS.h"
#include "task.h"
#include "pact_mem.h"
#include <errno.h>
#include <stddef.h>

/* 256K: the thumb cache builder's full-size RGB888 target (232px = 162K)
 * must fit alongside the library index. AXI budget check: 64K LVGL pool +
 * 256K here + 115K display buffers = 435K of 512K. */
#define PACT_HEAP_SIZE (256u * 1024u)

PACT_AXI static uint8_t heap_arena[PACT_HEAP_SIZE] __attribute__((aligned(8)));

void *__wrap__sbrk(ptrdiff_t incr)
{
    static ptrdiff_t brk;
    ptrdiff_t next = brk + incr;
    if (next < 0 || next > (ptrdiff_t)sizeof heap_arena) {
        errno = ENOMEM;
        return (void *)-1;
    }
    void *prev = &heap_arena[brk];
    brk = next;
    return prev;
}

struct _reent;

void __malloc_lock(struct _reent *reent)
{
    (void)reent;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskSuspendAll();
}

void __malloc_unlock(struct _reent *reent)
{
    (void)reent;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        (void)xTaskResumeAll();
}

#endif /* !PACT_SIM */
