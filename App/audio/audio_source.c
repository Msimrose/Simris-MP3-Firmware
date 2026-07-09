#include "audio_source.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *ext_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

audio_source_t *audio_source_open(const char *path)
{
    const char *e = ext_of(path);
    if (strcasecmp(e, "flac") == 0) return audio_source_open_flac(path);
    if (strcasecmp(e, "mp3")  == 0) return audio_source_open_mp3(path);
    if (strcasecmp(e, "wav")  == 0) return audio_source_open_wav(path);
    return NULL;
}

void audio_source_close(audio_source_t *s)
{
    if (s) s->close(s);
}
