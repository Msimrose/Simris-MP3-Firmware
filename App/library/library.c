#include "library.h"
#include "../pact_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifdef PACT_SIM
#include <dirent.h>
#include <sys/stat.h>
#endif

#define INDEX_MAGIC   "PACTIDX\x01"
#define MAX_DEPTH     12

/* Derived views: artist aggregation + global song order. Pure metadata
 * work: whatever the scanner found becomes the browse structure. */
static int artist_cmp(const void *a, const void *b)
{
    return strcasecmp(((const artist_t *)a)->name, ((const artist_t *)b)->name);
}

static library_t *songs_sort_lib;
static int song_cmp(const void *a, const void *b)
{
    const track_t *ta = &songs_sort_lib->tracks[*(const size_t *)a];
    const track_t *tb = &songs_sort_lib->tracks[*(const size_t *)b];
    int c = strcasecmp(ta->t.title, tb->t.title);
    if (c) return c;
    return strcasecmp(ta->t.artist, tb->t.artist);
}

static void build_views(library_t *lib)
{
    /* artists: group albums by artist name (case-insensitive) */
    for (size_t i = 0; i < lib->artist_count; i++) {
        free(lib->artists[i].albums);
        free(lib->artists[i].tracks);
    }
    free(lib->artists);
    lib->artists = NULL;
    lib->artist_count = 0;

    for (size_t a = 0; a < lib->album_count; a++) {
        const album_t *al = &lib->albums[a];
        artist_t *ar = NULL;
        for (size_t i = 0; i < lib->artist_count; i++) {
            if (strcasecmp(lib->artists[i].name, al->artist) == 0) {
                ar = &lib->artists[i];
                break;
            }
        }
        if (!ar) {
            lib->artists = realloc(lib->artists,
                                   (lib->artist_count + 1) * sizeof(artist_t));
            ar = &lib->artists[lib->artist_count++];
            memset(ar, 0, sizeof(*ar));
            ar->name = al->artist;
        }
        ar->albums = realloc(ar->albums, (ar->album_count + 1) * sizeof(size_t));
        ar->albums[ar->album_count++] = a;
        for (size_t t = al->first; t < al->first + al->count; t++) {
            ar->tracks = realloc(ar->tracks,
                                 (ar->track_count + 1) * sizeof(size_t));
            ar->tracks[ar->track_count++] = t;
        }
    }
    if (lib->artist_count)
        qsort(lib->artists, lib->artist_count, sizeof(artist_t), artist_cmp);
    /* re-point album indices after artist sort? not needed: albums[] holds
     * album indices which are unaffected by sorting the artists array */

    /* songs: every track, alphabetical by title */
    free(lib->songs);
    lib->songs = malloc(lib->count * sizeof(size_t));
    for (size_t i = 0; i < lib->count; i++) lib->songs[i] = i;
    songs_sort_lib = lib;
    if (lib->count) qsort(lib->songs, lib->count, sizeof(size_t), song_cmp);
}

static void build_albums(library_t *lib)
{
    free(lib->albums);
    lib->albums = NULL;
    lib->album_count = 0;
    for (size_t i = 0; i < lib->count; i++) {
        bool new_album =
            i == 0 ||
            strcasecmp(lib->tracks[i].t.album, lib->tracks[i - 1].t.album) != 0;
        if (new_album) {
            lib->albums = realloc(lib->albums,
                                  (lib->album_count + 1) * sizeof(album_t));
            album_t *al = &lib->albums[lib->album_count++];
            al->album = lib->tracks[i].t.album;
            al->artist = lib->tracks[i].t.artist;
            al->first = i;
            al->count = 1;
        } else {
            lib->albums[lib->album_count - 1].count++;
        }
    }
    build_views(lib);
}

/* ------------------------------ scan ------------------------------------ */
/* Directory walking is host-only for now; the device scan arrives with the
 * FatFs port (f_opendir/f_readdir) at storage bring-up. */
#ifdef PACT_SIM

static bool has_ext(const char *name, const char *ext)
{
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot + 1, ext) == 0;
}

static void push_track(library_t *lib, const char *path, const char *folder_art)
{
    track_t tr = {0};
    tr.path = strdup(path);
    if (!tags_read(path, &tr.t)) {
        free(tr.path);
        return;
    }
    if (tr.t.art_kind == ART_NONE && folder_art) {
        tr.t.art_kind = ART_FOLDER_FILE;
        tr.folder_art = strdup(folder_art);
    }
    lib->tracks = realloc(lib->tracks, (lib->count + 1) * sizeof(track_t));
    lib->tracks[lib->count++] = tr;
}

static void scan_dir(library_t *lib, const char *dir, int depth)
{
    if (depth > MAX_DEPTH) return;
    DIR *d = opendir(dir);
    if (!d) return;

    /* pass 1: find folder art in this directory */
    char folder_art[1024] = "";
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, "cover.jpg") == 0 ||
            strcasecmp(e->d_name, "folder.jpg") == 0 ||
            strcasecmp(e->d_name, "cover.png") == 0) {
            snprintf(folder_art, sizeof(folder_art), "%s/%s", dir, e->d_name);
            break;
        }
    }
    rewinddir(d);

    /* pass 2: tracks + recurse */
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_dir(lib, path, depth + 1);
        } else if (has_ext(e->d_name, "flac") || has_ext(e->d_name, "mp3") ||
                   has_ext(e->d_name, "wav")) {
            push_track(lib, path, folder_art[0] ? folder_art : NULL);
        }
    }
    closedir(d);
}

static int track_cmp(const void *a, const void *b)
{
    const track_t *ta = a, *tb = b;
    int c = strcasecmp(ta->t.album, tb->t.album);
    if (c) return c;
    c = strcasecmp(ta->t.artist, tb->t.artist);
    if (c) return c;
    if (ta->t.track_no != tb->t.track_no)
        return ta->t.track_no < tb->t.track_no ? -1 : 1;
    return strcasecmp(ta->path, tb->path);
}

bool library_scan(library_t *lib, const char *root)
{
    memset(lib, 0, sizeof(*lib));
    scan_dir(lib, root, 0);
    if (lib->count)
        qsort(lib->tracks, lib->count, sizeof(track_t), track_cmp);
    build_albums(lib);
    return true;
}

#else /* device: no filesystem walk until FatFs lands */

bool library_scan(library_t *lib, const char *root)
{
    (void)root;
    memset(lib, 0, sizeof(*lib));
    return false;
}

#endif /* PACT_SIM */

/* --------------------------- index save/load ---------------------------- */

static void w_str(pact_file_t *f, const char *s)
{
    uint16_t n = s ? (uint16_t)strlen(s) : 0;
    pact_write(f, &n, 2);
    if (n) pact_write(f, s, n);
}

static char *r_str(pact_file_t *f)
{
    uint16_t n;
    if (pact_read(f, &n, 2) != 2) return NULL;
    char *s = malloc((size_t)n + 1);
    if (n && pact_read(f, s, n) != n) { free(s); return NULL; }
    s[n] = '\0';
    return s;
}

bool library_save(const library_t *lib, const char *index_path)
{
    pact_file_t *f = pact_open_write(index_path);
    if (!f) return false;
    pact_write(f, INDEX_MAGIC, 8);
    uint32_t n = (uint32_t)lib->count;
    pact_write(f, &n, 4);
    for (size_t i = 0; i < lib->count; i++) {
        const track_t *tr = &lib->tracks[i];
        w_str(f, tr->path);
        w_str(f, tr->t.title);
        w_str(f, tr->t.artist);
        w_str(f, tr->t.album);
        w_str(f, tr->t.art_mime);
        w_str(f, tr->folder_art);
        pact_write(f, &tr->t.track_no, 2);
        pact_write(f, &tr->t.duration_ms, 4);
        pact_write(f, &tr->t.sample_rate, 4);
        pact_write(f, &tr->t.bits_per_sample, 1);
        uint8_t ak = (uint8_t)tr->t.art_kind;
        pact_write(f, &ak, 1);
        pact_write(f, &tr->t.art_offset, 8);
        pact_write(f, &tr->t.art_size, 4);
    }
    pact_close(f);
    return true;
}

bool library_load(library_t *lib, const char *index_path)
{
    memset(lib, 0, sizeof(*lib));
    pact_file_t *f = pact_open(index_path);
    if (!f) return false;

    char magic[8];
    uint32_t n;
    if (pact_read(f, magic, 8) != 8 || memcmp(magic, INDEX_MAGIC, 8) != 0 ||
        pact_read(f, &n, 4) != 4) {
        pact_close(f);
        return false;
    }

    lib->tracks = calloc(n, sizeof(track_t));
    for (uint32_t i = 0; i < n; i++) {
        track_t *tr = &lib->tracks[i];
        char *s;
        tr->path = r_str(f);
        if (!tr->path) goto fail;
        if (!(s = r_str(f))) goto fail;
        snprintf(tr->t.title, TAG_STR_MAX, "%s", s); free(s);
        if (!(s = r_str(f))) goto fail;
        snprintf(tr->t.artist, TAG_STR_MAX, "%s", s); free(s);
        if (!(s = r_str(f))) goto fail;
        snprintf(tr->t.album, TAG_STR_MAX, "%s", s); free(s);
        if (!(s = r_str(f))) goto fail;
        snprintf(tr->t.art_mime, TAG_MIME_MAX, "%s", s); free(s);
        tr->folder_art = r_str(f);
        if (tr->folder_art && !tr->folder_art[0]) {
            free(tr->folder_art);
            tr->folder_art = NULL;
        }
        uint8_t ak;
        if (pact_read(f, &tr->t.track_no, 2) != 2 ||
            pact_read(f, &tr->t.duration_ms, 4) != 4 ||
            pact_read(f, &tr->t.sample_rate, 4) != 4 ||
            pact_read(f, &tr->t.bits_per_sample, 1) != 1 ||
            pact_read(f, &ak, 1) != 1 ||
            pact_read(f, &tr->t.art_offset, 8) != 8 ||
            pact_read(f, &tr->t.art_size, 4) != 4)
            goto fail;
        tr->t.art_kind = (art_kind_t)ak;
        lib->count = i + 1;
    }
    pact_close(f);
    build_albums(lib);
    return true;

fail:
    pact_close(f);
    library_free(lib);
    return false;
}

void library_free(library_t *lib)
{
    for (size_t i = 0; i < lib->count; i++) {
        free(lib->tracks[i].path);
        free(lib->tracks[i].folder_art);
    }
    free(lib->tracks);
    free(lib->albums);
    for (size_t i = 0; i < lib->artist_count; i++) {
        free(lib->artists[i].albums);
        free(lib->artists[i].tracks);
    }
    free(lib->artists);
    free(lib->songs);
    memset(lib, 0, sizeof(*lib));
}

/* ------------------------------ art ------------------------------------- */

bool library_extract_art(const track_t *tr, const char *out_path)
{
    if (tr->t.art_kind == ART_NONE) return false;

    const char *src_path = tr->t.art_kind == ART_FOLDER_FILE ? tr->folder_art
                                                             : tr->path;
    if (!src_path) return false;

    pact_file_t *in = pact_open(src_path);
    if (!in) return false;

    uint64_t off = 0, len;
    if (tr->t.art_kind == ART_EMBEDDED) {
        off = tr->t.art_offset;
        len = tr->t.art_size;
    } else {
        len = pact_size(in);
    }
    pact_seek(in, off);

    pact_file_t *out = pact_open_write(out_path);
    if (!out) { pact_close(in); return false; }

    uint8_t buf[8192];
    uint64_t left = len;
    while (left) {
        size_t chunk = left < sizeof(buf) ? (size_t)left : sizeof(buf);
        if (pact_read(in, buf, chunk) != chunk) break;
        pact_write(out, buf, chunk);
        left -= chunk;
    }
    pact_close(in);
    pact_close(out);
    return left == 0;
}
