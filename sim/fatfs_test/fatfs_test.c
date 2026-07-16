/*
 * Pact MP-1 - FatFs storage-stack test (host).
 *
 * Exercises the DEVICE storage path on the Mac: the real FatFs build
 * (exFAT + UTF-8 LFN + reentrancy config), the pact_io FatFs backend,
 * and the device library scanner (f_opendir/f_readdir) - everything
 * except the SDMMC silicon, which is replaced by a file-backed disk
 * image (sparse, 1 GiB, pread/pwrite diskio).
 *
 *   fatfs_test <music_dir> [image_path]
 *
 * Flow: create image -> f_mkfs exFAT -> mount "0:" -> copy the music
 * tree in (stdio reads, f_write writes) -> library_scan("0:") -> print +
 * self-checks -> index save/load round-trip THROUGH pact_io on the image
 * -> art extraction to the image, size-verified.
 *
 * Build: compiled WITHOUT PACT_SIM, so library.c and pact_io take their
 * device (FatFs) branches; PACT_FATFS_HOST selects pthread FatFs mutexes
 * (see lib/fatfs/ffsystem_pact.c and this dir's CMakeLists).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>

#include "ff.h"
#include "diskio.h"
#include "library/library.h"
#include "library/thumbcache.h"
#include "pact_io.h"
#include "host_walk.h"

#define IMG_SECTORS 8388608u   /* x512 = 4 GiB, sparse */
#define SS 512u

/* ---- diskio over a file-backed image (pdrv 0 only) ----------------------- */

static int img_fd = -1;

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0 && img_fd >= 0) ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0) return RES_PARERR;
    ssize_t want = (ssize_t)count * SS;
    return pread(img_fd, buff, (size_t)want, (off_t)sector * SS) == want
               ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0) return RES_PARERR;
    ssize_t want = (ssize_t)count * SS;
    return pwrite(img_fd, buff, (size_t)want, (off_t)sector * SS) == want
               ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
    case CTRL_SYNC:        return RES_OK;
    case GET_SECTOR_COUNT: *(LBA_t *)buff = IMG_SECTORS; return RES_OK;
    case GET_BLOCK_SIZE:   *(DWORD *)buff = 1;           return RES_OK;
    default:               return RES_PARERR;
    }
}

/* ---- copy the fixture tree into the image -------------------------------- */

static size_t copied_music;

static int want_file(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    if (!strcasecmp(dot + 1, "flac") || !strcasecmp(dot + 1, "mp3") ||
        !strcasecmp(dot + 1, "wav"))
        return 1;                                   /* music */
    if (!strcasecmp(name, "cover.jpg") || !strcasecmp(name, "folder.jpg") ||
        !strcasecmp(name, "cover.png"))
        return 2;                                   /* folder art */
    return 0;
}

static void copy_cb(const char *abs, const char *rel, int is_dir, void *user)
{
    (void)user;
    char dst[1024];
    snprintf(dst, sizeof dst, "0:/%s", rel);

    if (is_dir) {
        f_mkdir(dst);
        return;
    }
    const char *base = strrchr(rel, '/');
    base = base ? base + 1 : rel;
    int kind = want_file(base);
    if (!kind) return;

    FILE *in = fopen(abs, "rb");
    if (!in) { fprintf(stderr, "skip (unreadable): %s\n", abs); return; }
    FIL out;
    if (f_open(&out, dst, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        fprintf(stderr, "f_open write failed: %s\n", dst);
        fclose(in);
        exit(1);
    }
    static char buf[1 << 16];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        UINT bw = 0;
        if (f_write(&out, buf, (UINT)n, &bw) != FR_OK || bw != n) {
            fprintf(stderr, "f_write failed: %s\n", dst);
            exit(1);
        }
    }
    f_close(&out);
    fclose(in);
    if (kind == 1) copied_music++;
}

/* ---- checks --------------------------------------------------------------- */

static int fails;
#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (!(cond)) { fails++; printf("FAIL: " __VA_ARGS__); putchar('\n'); } \
    } while (0)

static const char *art_str(const track_t *t)
{
    switch (t->t.art_kind) {
    case ART_EMBEDDED:    return "embedded";
    case ART_FOLDER_FILE: return "folder";
    default:              return "-";
    }
}

static void verify_roundtrip(const library_t *a, const library_t *b)
{
    CHECK(a->count == b->count, "roundtrip count %zu vs %zu", a->count, b->count);
    if (a->count != b->count) return;
    for (size_t i = 0; i < a->count; i++) {
        const track_t *x = &a->tracks[i], *y = &b->tracks[i];
        CHECK(!strcmp(x->path, y->path) && !strcmp(x->t.title, y->t.title) &&
              !strcmp(x->t.artist, y->t.artist) && !strcmp(x->t.album, y->t.album) &&
              x->t.track_no == y->t.track_no &&
              x->t.duration_ms == y->t.duration_ms &&
              x->t.sample_rate == y->t.sample_rate &&
              x->t.bits_per_sample == y->t.bits_per_sample &&
              x->t.art_kind == y->t.art_kind &&
              x->t.art_offset == y->t.art_offset &&
              x->t.art_size == y->t.art_size,
              "roundtrip mismatch at %zu (%s)", i, x->path);
    }
    CHECK(a->album_count == b->album_count, "roundtrip albums %zu vs %zu",
          a->album_count, b->album_count);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <music_dir> [image_path]\n", argv[0]);
        return 2;
    }
    const char *src = argv[1];
    const char *img = argc > 2 ? argv[2] : "/tmp/pact_fatfs.img";

    img_fd = open(img, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (img_fd < 0 || ftruncate(img_fd, (off_t)IMG_SECTORS * SS) != 0) {
        perror("image");
        return 1;
    }

    static FATFS fs;
    static BYTE work[1 << 16];
    MKFS_PARM parm = { .fmt = FM_EXFAT };
    FRESULT fr = f_mkfs("0:", &parm, work, sizeof work);
    if (fr != FR_OK) { fprintf(stderr, "f_mkfs: %d\n", fr); return 1; }
    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) { fprintf(stderr, "f_mount: %d\n", fr); return 1; }
    printf("image: %s (exFAT, %u MiB)\n", img, IMG_SECTORS / 2048);

    if (host_walk(src, copy_cb, NULL) != 0) {
        fprintf(stderr, "cannot read %s\n", src);
        return 1;
    }
    printf("copied: %zu music files from %s\n", copied_music, src);
    CHECK(copied_music > 0, "no music files found in %s", src);

    /* the device scanner, over the image */
    library_t lib;
    CHECK(library_scan(&lib, "0:"), "library_scan failed");
    printf("\nscanned %zu tracks, %zu albums, %zu artists:\n",
           lib.count, lib.album_count, lib.artist_count);
    for (size_t i = 0; i < lib.count; i++) {
        const track_t *t = &lib.tracks[i];
        printf("  %-28s %-18s %-24s trk%-3u %5u.%01us %6uHz/%u %s\n",
               t->t.title, t->t.artist, t->t.album, t->t.track_no,
               t->t.duration_ms / 1000, (t->t.duration_ms % 1000) / 100,
               t->t.sample_rate, t->t.bits_per_sample, art_str(t));
    }

    CHECK(lib.count == copied_music, "scan found %zu of %zu copied tracks",
          lib.count, copied_music);
    for (size_t i = 0; i < lib.count; i++) {
        const track_t *t = &lib.tracks[i];
        CHECK(t->t.title[0], "empty title: %s", t->path);
        CHECK(t->t.sample_rate > 0, "no sample rate: %s", t->path);
        CHECK(t->t.duration_ms > 0, "no duration: %s", t->path);
    }

    /* index round-trip through pact_io = through the FatFs image */
    CHECK(library_save(&lib, "0:/pact.idx"), "index save failed");
    library_t lib2;
    CHECK(library_load(&lib2, "0:/pact.idx"), "index load failed");
    verify_roundtrip(&lib, &lib2);

    /* art extraction onto the image, size-verified */
    for (size_t i = 0; i < lib.count; i++) {
        const track_t *t = &lib.tracks[i];
        if (t->t.art_kind == ART_NONE) continue;
        CHECK(library_extract_art(t, "0:/art_out.bin"),
              "art extract failed: %s", t->path);
        pact_file_t *af = pact_open("0:/art_out.bin");
        CHECK(af != NULL, "art_out.bin missing");
        if (af) {
            if (t->t.art_kind == ART_EMBEDDED)
                CHECK(pact_size(af) == t->t.art_size,
                      "art size %llu vs %u for %s",
                      (unsigned long long)pact_size(af), t->t.art_size, t->path);
            pact_close(af);
        }
        printf("\nart check: %s (%s, %llu bytes)\n", t->path, art_str(t),
               (unsigned long long)(t->t.art_kind == ART_EMBEDDED
                                        ? t->t.art_size : 0));
        break;
    }

    /* thumb cache: build, then verify every size for every BASELINE-JPEG
     * album (PNG folder art and progressive JPEG are known-skipped -
     * TJPGD and the H7 HW codec both reject progressive) */
    size_t built = thumbcache_build(&lib, "0:/.pactart");
    size_t jpeg_albums = 0, skipped_src = 0;
    for (size_t a = 0; a < lib.album_count; a++) {
        const track_t *t = &lib.tracks[lib.albums[a].first];
        if (t->t.art_kind == ART_NONE) continue;

        /* walk the JPEG markers: SOF0/SOF1 = decodable, SOF2 = progressive */
        bool baseline = false;
        pact_file_t *sf = pact_open(t->t.art_kind == ART_EMBEDDED
                                        ? t->path : t->folder_art);
        if (sf) {
            uint64_t off = t->t.art_kind == ART_EMBEDDED ? t->t.art_offset : 0;
            uint8_t m[4];
            pact_seek(sf, off);
            if (pact_read(sf, m, 2) == 2 && m[0] == 0xFF && m[1] == 0xD8) {
                uint64_t p = off + 2;
                for (int hop = 0; hop < 64; hop++) {
                    pact_seek(sf, p);
                    if (pact_read(sf, m, 4) != 4 || m[0] != 0xFF) break;
                    if (m[1] == 0xC0 || m[1] == 0xC1) { baseline = true; break; }
                    if (m[1] == 0xC2 || m[1] == 0xDA) break;  /* prog / SOS */
                    p += 2 + (uint64_t)((m[2] << 8) | m[3]);
                }
            }
            pact_close(sf);
        }
        if (!baseline) { skipped_src++; continue; }
        jpeg_albums++;

        for (int s = 0; s < PACT_THUMB_SIZE_COUNT; s++) {
            int px = pact_thumb_sizes[s];
            char pb[96];
            const char *tp = thumbcache_file(&lib, a, px, "0:/.pactart",
                                             pb, sizeof pb);
            CHECK(tp != NULL, "missing thumb: album %zu px %d", a, px);
            if (!tp) continue;
            pact_file_t *tf = pact_open(tp);
            uint8_t hdr[54];
            bool hdr_ok = tf && pact_read(tf, hdr, 54) == 54 &&
                          hdr[0] == 'B' && hdr[1] == 'M';
            CHECK(hdr_ok, "bad BMP header: %s", pb);
            if (hdr_ok) {
                uint32_t w = hdr[18] | (hdr[19] << 8);
                uint32_t h = hdr[22] | (hdr[23] << 8);
                uint16_t bpp = hdr[28];
                CHECK((int)w == px && (int)h == px && bpp == 24,
                      "thumb %s is %ux%u/%u, want %d", pb, w, h, bpp, px);
                /* middle row should contain actual image data */
                uint32_t stride = ((w * 3) + 3u) & ~3u;
                static uint8_t rowbuf[1024];
                pact_seek(tf, 54 + (uint64_t)stride * (h / 2));
                size_t rn = pact_read(tf, rowbuf, stride);
                bool varied = false;
                for (size_t i = 3; i < rn && !varied; i++)
                    if (rowbuf[i] != rowbuf[i % 3]) varied = true;
                CHECK(varied, "thumb %s middle row is flat", pb);
            }
            if (tf) pact_close(tf);
        }
    }
    CHECK(built == jpeg_albums * PACT_THUMB_SIZE_COUNT,
          "built %zu thumbs, expected %zu (%zu JPEG albums x %d sizes)",
          built, jpeg_albums * PACT_THUMB_SIZE_COUNT, jpeg_albums,
          PACT_THUMB_SIZE_COUNT);
    printf("\nthumb cache: %zu BMPs built for %zu baseline-JPEG albums "
           "(of %zu; %zu skipped: progressive/PNG sources)\n",
           built, jpeg_albums, lib.album_count, skipped_src);

    library_free(&lib);
    library_free(&lib2);
    close(img_fd);

    if (fails) {
        printf("\nFATFS TEST: %d FAILURE(S)\n", fails);
        return 1;
    }
    printf("\nFATFS TEST: OK (device scan + pact_io + index round-trip on exFAT image)\n");
    return 0;
}
