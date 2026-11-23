// ds_tts.h - DeepSeek text to speech: POST /v1/tools/tts, audio streamed back.
#ifndef DS_TTS_H
#define DS_TTS_H

#include "ds_core.h"
#include "ds_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS_TTS_MAX_BILLED 10000
#define DS_TTS_MAX_BODY 65536

typedef struct ds_tts_options {
    const char *text;
    const char *voice;          /* "stella" | "tide" */
    const char *format;         /* "wav" | "pcm" */
    bool filter_markdown;
    long timeout_s;
} ds_tts_options;

/* What the worker prepares so the UI never touches the audio buffer:
 * the peaks a waveform draws, the duration, and the file it wrote. */
#define DS_TTS_PEAKS 256

typedef struct ds_tts_artifact {
    char path[512];
    float peaks[DS_TTS_PEAKS];
    int peak_count;
    double duration_s;
    int chunks, cached, generated;
} ds_tts_artifact;

typedef struct ds_tts_result {
    ds_buf audio;
    char content_type[160];
    ds_err err;
    bool ok;
} ds_tts_result;

void ds_tts_result_free(ds_tts_result *r);
bool ds_tts_run(const ds_config *cfg, const ds_tts_options *o, ds_tts_result *out);

#ifdef __cplusplus
}
#endif

#endif
