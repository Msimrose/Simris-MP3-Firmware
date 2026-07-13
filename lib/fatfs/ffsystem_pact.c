/* FatFs OS hooks for the Pact MP-1: heap for LFN buffers (FF_USE_LFN == 3).
 * Reentrancy stays off; storage_task is the single filesystem owner per the
 * task architecture in firmware-spec section 10. */
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
