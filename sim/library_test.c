/*
 * Pact MP-1 - library scanner test harness (host only).
 *
 *   library_test <music_dir> [--art N out.img] [--index path]
 *       scans the tree, prints every track and the album grouping, saves the
 *       index, reloads it, and verifies the round-trip matches field by field.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../App/library/library.h"

static const char *art_str(const track_t *t)
{
    switch (t->t.art_kind) {
    case ART_EMBEDDED:    return "embedded";
    case ART_FOLDER_FILE: return "folder";
    default:              return "-";
    }
}

static int verify_roundtrip(const library_t *a, const library_t *b)
{
    if (a->count != b->count) {
        printf("ROUNDTRIP FAIL: count %zu vs %zu\n", a->count, b->count);
        return 1;
    }
    for (size_t i = 0; i < a->count; i++) {
        const track_t *x = &a->tracks[i], *y = &b->tracks[i];
        if (strcmp(x->path, y->path) || strcmp(x->t.title, y->t.title) ||
            strcmp(x->t.artist, y->t.artist) || strcmp(x->t.album, y->t.album) ||
            x->t.track_no != y->t.track_no ||
            x->t.duration_ms != y->t.duration_ms ||
            x->t.sample_rate != y->t.sample_rate ||
            x->t.bits_per_sample != y->t.bits_per_sample ||
            x->t.art_kind != y->t.art_kind ||
            x->t.art_offset != y->t.art_offset ||
            x->t.art_size != y->t.art_size) {
            printf("ROUNDTRIP FAIL at track %zu (%s)\n", i, x->path);
            return 1;
        }
    }
    if (a->album_count != b->album_count) {
        printf("ROUNDTRIP FAIL: albums %zu vs %zu\n", a->album_count, b->album_count);
        return 1;
    }
    printf("index round-trip: OK (%zu tracks, %zu albums)\n",
           a->count, a->album_count);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <music_dir> [--art N out.img] [--index path]\n",
                argv[0]);
        return 2;
    }

    int art_idx = -1;
    const char *art_out = NULL, *index_path = "/tmp/pact_index.bin";
    for (int i = 2; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--art") && i + 2 < argc) {
            art_idx = atoi(argv[i + 1]);
            art_out = argv[i + 2];
        }
        if (!strcmp(argv[i], "--index")) index_path = argv[i + 1];
    }

    library_t lib;
    if (!library_scan(&lib, argv[1])) {
        fprintf(stderr, "scan failed\n");
        return 1;
    }

    printf("%-3s %-24s %-18s %-20s %3s %8s %9s %4s %s\n", "#", "title", "artist",
           "album", "trk", "dur", "rate", "bits", "art");
    for (size_t i = 0; i < lib.count; i++) {
        const track_t *t = &lib.tracks[i];
        printf("%-3zu %-24s %-18s %-20s %3u %5u.%02us %6u Hz %4u %s\n", i,
               t->t.title, t->t.artist, t->t.album, t->t.track_no,
               t->t.duration_ms / 1000, (t->t.duration_ms % 1000) / 10,
               t->t.sample_rate, t->t.bits_per_sample, art_str(t));
    }
    printf("\nalbums:\n");
    for (size_t i = 0; i < lib.album_count; i++)
        printf("  [%zu] %s - %s (%zu tracks)\n", i, lib.albums[i].artist,
               lib.albums[i].album, lib.albums[i].count);

    if (art_idx >= 0 && art_idx < (int)lib.count) {
        if (library_extract_art(&lib.tracks[art_idx], art_out))
            printf("art extracted: track %d -> %s\n", art_idx, art_out);
        else
            printf("art extract FAILED for track %d\n", art_idx);
    }

    if (!library_save(&lib, index_path)) {
        fprintf(stderr, "index save failed\n");
        return 1;
    }
    library_t lib2;
    if (!library_load(&lib2, index_path)) {
        fprintf(stderr, "index load failed\n");
        return 1;
    }
    int rc = verify_roundtrip(&lib, &lib2);
    library_free(&lib);
    library_free(&lib2);
    return rc;
}
