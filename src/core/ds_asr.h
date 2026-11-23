// ds_asr.h - DeepSeek speech recognition: POST /v1/tools/asr (multipart file in,
// JSON {request_id, text, duration_ms} out).
#ifndef DS_ASR_H
#define DS_ASR_H

#include "ds_core.h"
#include "ds_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS_ASR_MAX_BYTES (8 * 1024 * 1024)

typedef struct ds_asr_result {
    char *text;
    int64_t duration_ms;
    char request_id[128];
    ds_err err;
    bool ok;
} ds_asr_result;

void ds_asr_result_free(ds_asr_result *r);
bool ds_asr_run(const ds_config *cfg, const char *path, long timeout_s, ds_asr_result *out);

#ifdef __cplusplus
}
#endif

#endif
