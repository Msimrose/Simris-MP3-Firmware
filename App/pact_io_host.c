/* Host (simulator/test) backend for pact_io: plain stdio. */
#ifdef PACT_SIM

#include "pact_io.h"
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

struct pact_file {
    FILE    *fp;
    uint64_t size;
};

static pact_file_t *open_mode(const char *path, const char *mode)
{
    FILE *fp = fopen(path, mode);
    if (!fp) return NULL;
    pact_file_t *f = malloc(sizeof(*f));
    f->fp = fp;
    fseeko(fp, 0, SEEK_END);
    f->size = (uint64_t)ftello(fp);
    fseeko(fp, 0, SEEK_SET);
    return f;
}

pact_file_t *pact_open(const char *path)       { return open_mode(path, "rb"); }
pact_file_t *pact_open_write(const char *path) { return open_mode(path, "wb"); }

void pact_close(pact_file_t *f)
{
    if (!f) return;
    fclose(f->fp);
    free(f);
}

size_t pact_read(pact_file_t *f, void *buf, size_t n)
{
    return fread(buf, 1, n, f->fp);
}

size_t pact_write(pact_file_t *f, const void *buf, size_t n)
{
    size_t w = fwrite(buf, 1, n, f->fp);
    uint64_t pos = (uint64_t)ftello(f->fp);
    if (pos > f->size) f->size = pos;
    return w;
}

bool pact_seek(pact_file_t *f, uint64_t offset)
{
    return fseeko(f->fp, (off_t)offset, SEEK_SET) == 0;
}

uint64_t pact_tell(pact_file_t *f)
{
    return (uint64_t)ftello(f->fp);
}

uint64_t pact_size(pact_file_t *f)
{
    return f->size;
}

bool pact_mkdir(const char *path)
{
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

#endif /* PACT_SIM */
