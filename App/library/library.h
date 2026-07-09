/*
 * Pact MP-1 - music library: scan, model, persisted index.
 *
 * The scanner walks a directory tree, reads tags for every .flac/.mp3/.wav,
 * falls back to cover.jpg / folder.jpg for art, and sorts tracks into
 * album order. library_save/load round-trip the whole model through a
 * compact binary index so boot never rescans unchanged storage.
 *
 * Directory walking is POSIX here; the FatFs port swaps the internals of
 * library_scan() only.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "tags.h"

typedef struct {
    char        *path;            /* malloc'd */
    char        *folder_art;      /* malloc'd or NULL (ART_FOLDER_FILE) */
    track_tags_t t;
} track_t;

typedef struct {
    const char *album;            /* points into a track's tags */
    const char *artist;
    size_t      first;            /* index into tracks[] */
    size_t      count;
} album_t;

typedef struct {
    track_t *tracks;
    size_t   count;
    album_t *albums;
    size_t   album_count;
} library_t;

bool library_scan(library_t *lib, const char *root);
bool library_save(const library_t *lib, const char *index_path);
bool library_load(library_t *lib, const char *index_path);
void library_free(library_t *lib);

/* Copy a track's cover art (embedded bytes or folder file) to out_path.
 * Returns false if the track has no art. */
bool library_extract_art(const track_t *tr, const char *out_path);
