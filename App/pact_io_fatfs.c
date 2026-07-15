/* Device backend for pact_io: FatFs. Paths use FatFs volume syntax
 * ("0:/Music/x.flac" = microSD, "1:/..." = eMMC). */
#ifndef PACT_SIM

#include "pact_io.h"
#include "ff.h"
#include <stdlib.h>

struct pact_file {
    FIL fil;
};

static pact_file_t *open_mode(const char *path, BYTE mode)
{
    pact_file_t *f = malloc(sizeof(*f));
    if (!f) return NULL;
    if (f_open(&f->fil, path, mode) != FR_OK) {
        free(f);
        return NULL;
    }
    return f;
}

pact_file_t *pact_open(const char *path)
{
    return open_mode(path, FA_READ);
}

pact_file_t *pact_open_write(const char *path)
{
    return open_mode(path, FA_WRITE | FA_CREATE_ALWAYS);
}

void pact_close(pact_file_t *f)
{
    if (!f) return;
    f_close(&f->fil);
    free(f);
}

size_t pact_read(pact_file_t *f, void *buf, size_t n)
{
    UINT br = 0;
    if (f_read(&f->fil, buf, (UINT)n, &br) != FR_OK) return 0;
    return br;
}

size_t pact_write(pact_file_t *f, const void *buf, size_t n)
{
    UINT bw = 0;
    if (f_write(&f->fil, buf, (UINT)n, &bw) != FR_OK) return 0;
    return bw;
}

bool pact_seek(pact_file_t *f, uint64_t offset)
{
    return f_lseek(&f->fil, (FSIZE_t)offset) == FR_OK;
}

uint64_t pact_tell(pact_file_t *f)
{
    return (uint64_t)f_tell(&f->fil);
}

uint64_t pact_size(pact_file_t *f)
{
    return (uint64_t)f_size(&f->fil);
}

bool pact_mkdir(const char *path)
{
    FRESULT fr = f_mkdir(path);
    return fr == FR_OK || fr == FR_EXIST;
}

#endif /* !PACT_SIM */
