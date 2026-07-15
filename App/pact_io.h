/*
 * Pact MP-1 - file IO seam. Everything in App/ that touches storage goes
 * through this interface; the backend is stdio on the host and FatFs on
 * the device. Decoders and tag parsers never see the difference.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct pact_file pact_file_t;

pact_file_t *pact_open(const char *path);        /* read-only */
pact_file_t *pact_open_write(const char *path);  /* create/truncate */
void         pact_close(pact_file_t *f);
bool         pact_mkdir(const char *path);       /* true if created/exists */

size_t   pact_read(pact_file_t *f, void *buf, size_t n);   /* bytes read */
size_t   pact_write(pact_file_t *f, const void *buf, size_t n);
bool     pact_seek(pact_file_t *f, uint64_t offset);       /* absolute */
uint64_t pact_tell(pact_file_t *f);
uint64_t pact_size(pact_file_t *f);
