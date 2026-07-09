/*
 * Pact MP-1 - decode verification tool (host only).
 *
 *   decode_test <input> <out.raw> [--vol N] [--seek SECONDS]
 *       decode input through the real pipeline (source -> volume) to raw
 *       interleaved stereo s32le, printing the source format.
 *
 *   decode_test --compare <a.raw> <b.raw>
 *       sample-exact comparison; prints diffs, max error, SNR.
 *       exit 0 only when bit-identical.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../App/audio/audio_source.h"
#include "../App/audio/volume.h"

#define CHUNK 4096

static int compare(const char *pa, const char *pb)
{
    FILE *fa = fopen(pa, "rb"), *fb = fopen(pb, "rb");
    if (!fa || !fb) { fprintf(stderr, "open failed\n"); return 2; }

    static int32_t ba[CHUNK], bb[CHUNK];
    uint64_t n = 0, diffs = 0;
    int64_t max_err = 0;
    double sig = 0, noise = 0;

    for (;;) {
        size_t na = fread(ba, 4, CHUNK, fa);
        size_t nb = fread(bb, 4, CHUNK, fb);
        size_t m = na < nb ? na : nb;
        for (size_t i = 0; i < m; i++) {
            int64_t d = (int64_t)ba[i] - bb[i];
            if (d) { diffs++; if (llabs(d) > max_err) max_err = llabs(d); }
            sig += (double)ba[i] * ba[i];
            noise += (double)d * d;
        }
        n += m;
        if (na != nb) {
            printf("length mismatch after %llu samples (a:%zu b:%zu)\n",
                   (unsigned long long)n, na, nb);
            return 1;
        }
        if (na < CHUNK) break;
    }

    printf("samples: %llu  differing: %llu  max|err|: %lld\n",
           (unsigned long long)n, (unsigned long long)diffs, (long long)max_err);
    if (diffs == 0) {
        printf("BIT-PERFECT MATCH\n");
        return 0;
    }
    double snr = (noise > 0 && sig > 0) ? 10.0 * log10(sig / noise) : 0.0;
    printf("SNR vs reference: %.1f dB\n", snr);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc >= 4 && strcmp(argv[1], "--compare") == 0)
        return compare(argv[2], argv[3]);

    if (argc < 3) {
        fprintf(stderr, "usage: %s <in> <out.raw> [--vol N] [--seek S]\n"
                        "       %s --compare <a.raw> <b.raw>\n",
                argv[0], argv[0]);
        return 2;
    }

    int vol = VOLUME_STEPS - 1;
    double seek_s = -1.0;
    for (int i = 3; i < argc - 1; i++) {
        if (strcmp(argv[i], "--vol") == 0)  vol = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--seek") == 0) seek_s = atof(argv[i + 1]);
    }

    audio_source_t *src = audio_source_open(argv[1]);
    if (!src) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }

    fprintf(stderr, "%s: %u Hz, %u ch, %u bit, %llu frames\n", argv[1],
            src->fmt.sample_rate, src->fmt.channels, src->fmt.bits_per_sample,
            (unsigned long long)src->fmt.total_frames);

    if (seek_s >= 0 &&
        !src->seek(src, (uint64_t)(seek_s * src->fmt.sample_rate))) {
        fprintf(stderr, "seek failed\n");
        return 1;
    }

    FILE *out = fopen(argv[2], "wb");
    if (!out) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }

    static int32_t buf[CHUNK * 2];
    uint64_t total = 0;
    for (;;) {
        size_t got = src->read(src, buf, CHUNK);
        if (!got) break;
        volume_apply(buf, got * 2, vol);
        fwrite(buf, sizeof(int32_t) * 2, got, out);
        total += got;
        if (got < CHUNK) break;
    }
    fclose(out);
    audio_source_close(src);
    fprintf(stderr, "decoded %llu frames\n", (unsigned long long)total);
    return 0;
}
