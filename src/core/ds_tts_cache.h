// ds_tts_cache.h - synthesize a long text in sentence-sized chunks and cache
// each chunk, so editing one sentence re-synthesizes only that chunk instead of
// the whole text.
//
// Trade-off, stated plainly: chunking costs one request per chunk, so a first
// generation of a long text is slower than a single request. The win is on the
// second and later generation, which is what editing a long text actually does.
#ifndef DS_TTS_CACHE_H
#define DS_TTS_CACHE_H

#include "ds_core.h"
#include "ds_tts.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_tts_chunk_result {
    ds_buf audio;
    char content_type[160];
    int chunks;        /* how many chunks the text was split into */
    int cached;        /* how many came from the cache */
    int generated;     /* how many were synthesized */
    ds_err err;
    bool ok;
} ds_tts_chunk_result;

void ds_tts_chunk_result_free(ds_tts_chunk_result *r);

/* Progress: called on the calling thread as each chunk lands. */
typedef void (*ds_tts_progress)(void *user, int done, int total);

bool ds_tts_synth_cached(const ds_config *cfg, const char *text, const char *voice,
                         const char *format, bool filter_markdown,
                         ds_tts_chunk_result *out,
                         ds_tts_progress progress, void *user);

/* Split text into chunk byte ranges at sentence boundaries. */
int ds_tts_split(const char *text, int max_chars,
                 int *starts, int *lens, int max_chunks);

const char *ds_cache_dir(void);
bool ds_cache_clear(char *err, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
