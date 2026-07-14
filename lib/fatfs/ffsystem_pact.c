/* FatFs OS hooks for the Pact MP-1.
 *
 * Heap: LFN working buffers (FF_USE_LFN == 3) come from malloc - on the
 * device that is the AXI arena (App/hal/pact_heap.c).
 *
 * Mutexes (FF_FS_REENTRANT == 1): FatFs is called from several tasks -
 * audio (decoder reads the playing file), ui (track open on play),
 * storage (library scan + index write) - so the per-volume locks are
 * load-bearing, not optional. The earlier "storage_task is the single
 * filesystem owner" note predated the audio/ui paths and was wrong.
 * Indices 0..FF_VOLUMES-1 are the volume locks; FF_VOLUMES is FatFs'
 * system lock (both created by f_mount).
 *
 * PACT_FATFS_HOST builds this same file for the host storage-stack test
 * (sim/fatfs_test/) with pthread mutexes instead of FreeRTOS.
 */
#include "ff.h"

#if FF_USE_LFN == 3
#include <stdlib.h>

void *ff_memalloc(UINT msize)
{
    return malloc(msize);
}

void ff_memfree(void *mblock)
{
    free(mblock);
}
#endif

#if FF_FS_REENTRANT

#ifdef PACT_FATFS_HOST

#include <pthread.h>

static pthread_mutex_t ff_mtx[FF_VOLUMES + 1];

int ff_mutex_create(int vol)
{
    return pthread_mutex_init(&ff_mtx[vol], NULL) == 0;
}

void ff_mutex_delete(int vol)
{
    pthread_mutex_destroy(&ff_mtx[vol]);
}

int ff_mutex_take(int vol)
{
    return pthread_mutex_lock(&ff_mtx[vol]) == 0;
}

void ff_mutex_give(int vol)
{
    pthread_mutex_unlock(&ff_mtx[vol]);
}

#else /* device: FreeRTOS mutexes (priority inheritance matters - the
         audio task must not be starved by a storage-task scan) */

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t ff_mtx[FF_VOLUMES + 1];

int ff_mutex_create(int vol)
{
    ff_mtx[vol] = xSemaphoreCreateMutex();
    return ff_mtx[vol] != NULL;
}

void ff_mutex_delete(int vol)
{
    vSemaphoreDelete(ff_mtx[vol]);
    ff_mtx[vol] = NULL;
}

int ff_mutex_take(int vol)
{
    return xSemaphoreTake(ff_mtx[vol], FF_FS_TIMEOUT) == pdTRUE;
}

void ff_mutex_give(int vol)
{
    xSemaphoreGive(ff_mtx[vol]);
}

#endif /* PACT_FATFS_HOST */

#endif /* FF_FS_REENTRANT */
