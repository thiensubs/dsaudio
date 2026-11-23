#include "ds_asr.h"

#include "ds_audio.h"
#include "ds_media.h"
#include "ds_http.h"
#include "ds_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ds_asr_result_free(ds_asr_result *r) {
    if (!r) return;
    free(r->text);
    memset(r, 0, sizeof *r);
}

bool ds_asr_run(const ds_config *cfg, const char *path, long timeout_s, ds_asr_result *out) {
    memset(out, 0, sizeof *out);
    out->ok = false;

    if (!cfg || !cfg->have_key) {
        ds_err_set(&out->err, "no DeepSeek key: set DEEPSEEK_API_KEY or add one in Settings");
        return false;
    }
    if (!path || !path[0]) {
        ds_err_set(&out->err, "no audio file");
        return false;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        ds_err_set(&out->err, "cannot open %s", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        ds_err_set(&out->err, "%s is empty", path);
        return false;
    }
    if (size > DS_ASR_MAX_BYTES) {
        fclose(f);
        ds_err_set(&out->err, "%s is %.1f MiB, the limit is 8 MiB", path, size / 1048576.0);
        return false;
    }
    unsigned char *file = (unsigned char *)malloc((size_t)size);
    if (!file) {
        fclose(f);
        ds_err_set(&out->err, "out of memory reading %s", path);
        return false;
    }
    size_t got = fread(file, 1, (size_t)size, f);
    fclose(f);

    ds_buf wav;
    const unsigned char *payload = file;
    size_t payload_len = got;
    char up_name[600];
    snprintf(up_name, sizeof up_name, "%s", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    bool converted = false;

    if (!ds_media_is_asr_ready(file, got)) {
        /* MP3, FLAC and friends: decode to PCM WAV, which ASR takes. */
        if (!ds_media_decode_to_wav(path, &wav, out->err.text, sizeof out->err.text)) {
            free(file);
            out->ok = false;
            return false;
        }
        converted = true;
        payload = wav.data;
        payload_len = wav.len;
        char base[512];
        snprintf(base, sizeof base, "%s", up_name);
        char *dot = strrchr(base, '.');
        if (dot) *dot = '\0';
        snprintf(up_name, sizeof up_name, "%s.wav", base);
    }

    char url[512];
    snprintf(url, sizeof url, "%s/v1/tools/asr", cfg->base_url);

    ds_http_response resp;
    long timeout = timeout_s > 0 ? timeout_s : 600;
    bool sent;
    if (converted)
        sent = ds_http_post_mem(url, cfg->api_key, up_name, payload, payload_len, "file", timeout, &resp);
    else
        sent = ds_http_post_mem(url, cfg->api_key, up_name, payload, payload_len, "file", timeout, &resp);
    free(file);
    if (converted) ds_buf_free(&wav);
    if (!sent) {
        ds_err_set(&out->err, "%s", resp.err);
        ds_http_response_free(&resp);
        return false;
    }

    if (resp.status < 200 || resp.status >= 300) {
        ds_json *j = ds_json_parse(resp.body ? resp.body : "", resp.body_len);
        char msg[320] = "";
        if (j) {
            if (!ds_json_str(j, "message", msg, sizeof msg))
                ds_json_str(j, "error", msg, sizeof msg);
            ds_json_free(j);
        }
        if (!msg[0])
            snprintf(msg, sizeof msg, "%.*s", 200, resp.body ? resp.body : "");
        ds_err_set(&out->err, "HTTP %ld: %s", resp.status, msg);
        ds_http_response_free(&resp);
        return false;
    }

    ds_json *j = ds_json_parse(resp.body ? resp.body : "", resp.body_len);
    if (!j) {
        ds_err_set(&out->err, "the reply was not JSON: %.200s", resp.body ? resp.body : "");
        ds_http_response_free(&resp);
        return false;
    }

    char text[16384];
    if (!ds_json_str(j, "text", text, sizeof text)) text[0] = '\0';
    int64_t dur = 0;
    ds_json_i64(j, "duration_ms", &dur);
    char req_id[128];
    ds_json_str(j, "request_id", req_id, sizeof req_id);
    ds_json_free(j);
    ds_http_response_free(&resp);

    out->text = (char *)malloc(strlen(text) + 1);
    if (!out->text) {
        ds_err_set(&out->err, "out of memory");
        return false;
    }
    strcpy(out->text, text);
    out->duration_ms = dur;
    snprintf(out->request_id, sizeof out->request_id, "%s", req_id);
    out->ok = true;
    return true;
}
