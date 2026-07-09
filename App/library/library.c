#include "library.h"
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

static void w_str(FILE *f, const char *s)
{
    uint16_t n = s ? (uint16_t)strlen(s) : 0;
    fwrite(&n, 2, 1, f);
    if (n) fwrite(s, 1, n, f);
}

static char *r_str(FILE *f)
{
    uint16_t n;
    if (fread(&n, 2, 1, f) != 1) return NULL;
    char *s = malloc((size_t)n + 1);
    if (n && fread(s, 1, n, f) != n) { free(s); return NULL; }
    s[n] = '\0';
    return s;
}

bool library_save(const library_t *lib, const char *index_path)
{
    FILE *f = fopen(index_path, "wb");
    if (!f) return false;
    fwrite(INDEX_MAGIC, 1, 8, f);
    uint32_t n = (uint32_t)lib->count;
    fwrite(&n, 4, 1, f);
    for (size_t i = 0; i < lib->count; i++) {
        const track_t *tr = &lib->tracks[i];
        w_str(f, tr->path);
        w_str(f, tr->t.title);
        w_str(f, tr->t.artist);
        w_str(f, tr->t.album);
        w_str(f, tr->t.art_mime);
        w_str(f, tr->folder_art);
        fwrite(&tr->t.track_no, 2, 1, f);
        fwrite(&tr->t.duration_ms, 4, 1, f);
        fwrite(&tr->t.sample_rate, 4, 1, f);
        fwrite(&tr->t.bits_per_sample, 1, 1, f);
        uint8_t ak = (uint8_t)tr->t.art_kind;
        fwrite(&ak, 1, 1, f);
        fwrite(&tr->t.art_offset, 8, 1, f);
        fwrite(&tr->t.art_size, 4, 1, f);
    }
    fclose(f);
    return true;
}

bool library_load(library_t *lib, const char *index_path)
{
    memset(lib, 0, sizeof(*lib));
    FILE *f = fopen(index_path, "rb");
    if (!f) return false;

    char magic[8];
    uint32_t n;
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, INDEX_MAGIC, 8) != 0 ||
        fread(&n, 4, 1, f) != 1) {
        fclose(f);
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
        if (fread(&tr->t.track_no, 2, 1, f) != 1 ||
            fread(&tr->t.duration_ms, 4, 1, f) != 1 ||
            fread(&tr->t.sample_rate, 4, 1, f) != 1 ||
            fread(&tr->t.bits_per_sample, 1, 1, f) != 1 ||
            fread(&ak, 1, 1, f) != 1 ||
            fread(&tr->t.art_offset, 8, 1, f) != 1 ||
            fread(&tr->t.art_size, 4, 1, f) != 1)
            goto fail;
        tr->t.art_kind = (art_kind_t)ak;
        lib->count = i + 1;
    }
    fclose(f);
    build_albums(lib);
    return true;

fail:
    fclose(f);
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
    memset(lib, 0, sizeof(*lib));
}

/* ------------------------------ art ------------------------------------- */

bool library_extract_art(const track_t *tr, const char *out_path)
{
    if (tr->t.art_kind == ART_NONE) return false;

    const char *src_path = tr->t.art_kind == ART_FOLDER_FILE ? tr->folder_art
                                                             : tr->path;
    if (!src_path) return false;

    FILE *in = fopen(src_path, "rb");
    if (!in) return false;

    uint64_t off = 0, len;
    if (tr->t.art_kind == ART_EMBEDDED) {
        off = tr->t.art_offset;
        len = tr->t.art_size;
    } else {
        fseek(in, 0, SEEK_END);
        len = (uint64_t)ftell(in);
    }
    fseek(in, (long)off, SEEK_SET);

    FILE *out = fopen(out_path, "wb");
    if (!out) { fclose(in); return false; }

    uint8_t buf[8192];
    uint64_t left = len;
    while (left) {
        size_t chunk = left < sizeof(buf) ? (size_t)left : sizeof(buf);
        if (fread(buf, 1, chunk, in) != chunk) break;
        fwrite(buf, 1, chunk, out);
        left -= chunk;
    }
    fclose(in);
    fclose(out);
    return left == 0;
}
