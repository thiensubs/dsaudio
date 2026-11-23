#include "ds_tts.h"

#include "ds_http.h"
#include "ds_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ds_tts_result_free(ds_tts_result *r) {
    if (!r) return;
    ds_buf_free(&r->audio);
    memset(r, 0, sizeof *r);
}

static bool is_json_content(const char *ctype) {
    return ctype && strstr(ctype, "json") != NULL;
}

static void extract_error(const char *body, char *out, size_t cap) {
    ds_json *j = ds_json_parse(body, strlen(body));
    if (!j) {
        snprintf(out, cap, "%.*s", (int)(cap - 1), body ? body : "");
        return;
    }
    char msg[320] = "";
    if (!ds_json_str(j, "message", msg, sizeof msg))
        ds_json_str(j, "error", msg, sizeof msg);
    ds_json_free(j);
    if (msg[0]) {
        snprintf(out, cap, "%s", msg);
    } else {
        snprintf(out, cap, "%.*s", (int)(cap - 1), body ? body : "");
    }
}

bool ds_tts_run(const ds_config *cfg, const ds_tts_options *o, ds_tts_result *out) {
    memset(out, 0, sizeof *out);
    out->ok = false;

    if (!cfg || !cfg->have_key) {
        ds_err_set(&out->err, "no DeepSeek key: set DEEPSEEK_API_KEY or add one in Settings");
        return false;
    }
    if (!o || !o->text || !o->text[0]) {
        ds_err_set(&out->err, "no text to read");
        return false;
    }

    size_t billed = ds_utf8_billed_len(o->text);
    if (billed > DS_TTS_MAX_BILLED) {
        ds_err_set(&out->err, "text is %zu billed characters, the limit is %d",
                   billed, DS_TTS_MAX_BILLED);
        return false;
    }

    const char *voice = (o->voice && o->voice[0]) ? o->voice : "stella";
    const char *format = (o->format && o->format[0]) ? o->format : "wav";
    if (strcmp(voice, "stella") != 0 && strcmp(voice, "tide") != 0) {
        ds_err_set(&out->err, "unknown voice '%s' (use stella or tide)", voice);
        return false;
    }
    if (strcmp(format, "wav") != 0 && strcmp(format, "pcm") != 0) {
        ds_err_set(&out->err, "unknown format '%s' (use wav or pcm)", format);
        return false;
    }

    ds_jsonw *w = ds_jsonw_new();
    ds_jsonw_set_str(w, "text", o->text);
    ds_jsonw_set_str(w, "voice_id", voice);
    ds_jsonw_set_str(w, "format", format);
    ds_jsonw_set_bool(w, "filter_markdown", o->filter_markdown);
    char *body = ds_jsonw_finish(w);
    if (!body) {
        ds_err_set(&out->err, "out of memory building the request");
        return false;
    }
    size_t body_len = strlen(body);
    if (body_len > DS_TTS_MAX_BODY) {
        free(body);
        ds_err_set(&out->err, "request body is %zu bytes, the limit is %d",
                   body_len, DS_TTS_MAX_BODY);
        return false;
    }

    char url[512];
    snprintf(url, sizeof url, "%s/v1/tools/tts", cfg->base_url);

    ds_http_response resp;
    long timeout = o->timeout_s > 0 ? o->timeout_s : 180;
    bool sent = ds_http_post_json(url, cfg->api_key, body, timeout, &resp);
    free(body);

    if (!sent) {
        ds_err_set(&out->err, "%s", resp.err);
        ds_http_response_free(&resp);
        return false;
    }

    bool ok = true;
    if (resp.status < 200 || resp.status >= 300) {
        char msg[320];
        extract_error(resp.body ? resp.body : "", msg, sizeof msg);
        ds_err_set(&out->err, "HTTP %ld: %s", resp.status, msg);
        ok = false;
    } else if (is_json_content(resp.content_type)) {
        char msg[320];
        extract_error(resp.body ? resp.body : "", msg, sizeof msg);
        ds_err_set(&out->err, "unexpected JSON reply: %s", msg);
        ok = false;
    } else if (!resp.body || resp.body_len == 0) {
        ds_err_set(&out->err, "empty audio reply (a truncated response is a failure)");
        ok = false;
    }

    if (ok) {
        out->audio.data = (unsigned char *)resp.body;
        out->audio.len = resp.body_len;
        out->audio.cap = resp.body_len + 1;
        snprintf(out->content_type, sizeof out->content_type, "%s",
                 resp.content_type[0] ? resp.content_type : "audio");
        resp.body = NULL;
        out->ok = true;
    }
    ds_http_response_free(&resp);
    return ok;
}
