#include "app_state.h"

#include "../core/ds_i18n.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void stamp_now(char *out, size_t cap) {
    time_t t = time(NULL);
    struct tm tm;
    ds_localtime(t, &tm);
    strftime(out, cap, "%H:%M:%S", &tm);
}

void app_state_request_tts(app_state *a) {
    if (ds_job_is_running(&a->job)) return;
    const char *voice = a->tts_voice == 1 ? "tide" : "stella";
    const char *format = a->tts_format == 1 ? "pcm" : "wav";
    ds_job_start_tts(&a->job, &a->cfg, a->tts_text, voice, format, a->tts_filter);
    a->tts_dirty = false;
    app_state_set_status(a, "generating...");
}

void app_state_request_asr(app_state *a) {
    if (ds_job_is_running(&a->job)) return;
    if (!a->asr_path[0]) return;
    ds_job_start_asr(&a->job, &a->cfg, a->asr_path);
    app_state_set_status(a, "transcribing...");
}

void app_state_set_status(app_state *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->status, sizeof a->status, fmt, ap);
    va_end(ap);
}

void app_state_add_history(app_state *a, const char *kind, const char *detail,
                           const char *text, size_t bytes, int status, const char *note) {
    int cap = a->cfg.history_limit > 0 ? a->cfg.history_limit : 64;
    int total = (int)(sizeof a->history / sizeof a->history[0]);
    if (cap > total) cap = total;
    history_item *h;
    if (a->history_count < cap) {
        h = &a->history[a->history_count++];
    } else {
        memmove(&a->history[0], &a->history[1], sizeof a->history[0] * (cap - 1));
        h = &a->history[cap - 1];
    }
    memset(h, 0, sizeof *h);
    stamp_now(h->stamp, sizeof h->stamp);
    snprintf(h->kind, sizeof h->kind, "%s", kind);
    snprintf(h->detail, sizeof h->detail, "%s", detail);
    snprintf(h->text, sizeof h->text, "%s", text);
    h->bytes = bytes;
    h->status = status;
    snprintf(h->note, sizeof h->note, "%s", note);
    a->history_sel = (int)(a->history_count - 1);
}

void app_state_init(app_state *a) {
    memset(a, 0, sizeof *a);
    ds_config_load(&a->cfg);
    ds_job_init(&a->job);
    a->player = ds_player_create();
    a->page = PAGE_TTS;
    a->tts_voice = strcmp(a->cfg.default_voice, "tide") == 0 ? 1 : 0;
    a->tts_format = strcmp(a->cfg.default_format, "pcm") == 0 ? 1 : 0;
    a->tts_filter = a->cfg.filter_markdown;
    a->theme_mode = a->cfg.theme;
    a->context_open = false;

    snprintf(a->tts_text, sizeof a->tts_text,
             "DeepSeek text to speech. Edit this text, pick a voice, then press Generate.\n"
             "The audio comes back as a waveform you can scrub, and the text stays editable "
             "so a mistake is one keystroke from fixed.");
    snprintf(a->set_key, sizeof a->set_key, "%s", a->cfg.api_key);
    snprintf(a->set_base, sizeof a->set_base, "%s", a->cfg.base_url);
    snprintf(a->set_out, sizeof a->set_out, "%s", a->cfg.output_dir);
    a->set_tts_timeout = a->cfg.tts_timeout_s;
    a->set_asr_timeout = a->cfg.asr_timeout_s;
    a->set_history_limit = a->cfg.history_limit;
    a->set_peaks = a->cfg.peaks;
    a->set_language = ds_language_index(a->cfg.language);
    ds_i18n_set(a->set_language >= 0 ? a->set_language : DS_LANG_EN);

    char masked[128];
    ds_config_mask_key(&a->cfg, masked, sizeof masked);
    app_state_set_status(a, "ready - key %s", masked);
}

void app_state_free(app_state *a) {
    ds_tts_result_free(&a->tts);
    ds_asr_result_free(&a->job.asr);
    ds_job_shutdown(&a->job);
    if (a->player) {
        ds_player_destroy(a->player);
        a->player = NULL;
    }
}

void app_state_poll_job(app_state *a) {
    if (!ds_job_poll(&a->job)) return;

    if (a->job.kind == DS_JOB_TTS) {
        ds_tts_result_free(&a->tts);
        a->tts = a->job.tts;
        memset(&a->job.tts, 0, sizeof a->job.tts);
        a->tts_dirty = false;
        a->artifact = a->job.artifact;
        memset(&a->job.artifact, 0, sizeof a->job.artifact);
        a->tts_peak_count = a->artifact.peak_count;
        memcpy(a->tts_peaks, a->artifact.peaks, sizeof a->tts_peaks);
        a->tts_duration_s = a->artifact.duration_s;
        a->tts_progress = 0.0f;
        snprintf(a->tts_path, sizeof a->tts_path, "%s", a->artifact.path);
        if (a->tts.ok) {
            const char *voice = a->tts_voice == 1 ? "tide" : "stella";
            const char *format = a->tts_format == 1 ? "pcm" : "wav";
            char detail[64];
            snprintf(detail, sizeof detail, "%s / %s", voice, format);
            app_state_add_history(a, "tts", detail, a->tts_text, a->tts.audio.len, 0,
                                  a->tts.content_type);
            app_state_set_status(a, "tts ok - %zu bytes in %.1fs", a->tts.audio.len, a->job.elapsed);
        } else {
            app_state_add_history(a, "tts", "failed", a->tts_text, 0, 1, a->tts.err.text);
            app_state_set_status(a, "tts failed: %s", a->tts.err.text);
        }
    } else if (a->job.kind == DS_JOB_ASR) {
        ds_asr_result r = a->job.asr;
        memset(&a->job.asr, 0, sizeof a->job.asr);
        if (r.ok) {
            snprintf(a->asr_transcript, sizeof a->asr_transcript, "%s", r.text);
            snprintf(a->asr_original, sizeof a->asr_original, "%s", r.text);
            a->asr_dirty = false;
            a->asr_duration_ms = r.duration_ms;
            snprintf(a->asr_request_id, sizeof a->asr_request_id, "%s", r.request_id);
            app_state_add_history(a, "asr", a->asr_path, r.text, strlen(r.text), 0,
                                  a->asr_request_id);
            app_state_set_status(a, "asr ok - %lld ms in %.1fs",
                                 (long long)r.duration_ms, a->job.elapsed);
            ds_log_add(DS_LOG_OK, "asr: %zu chars from %s", strlen(r.text), a->asr_path);
        } else {
            app_state_add_history(a, "asr", a->asr_path, "", 0, 1, r.err.text);
            app_state_set_status(a, "asr failed: %s", r.err.text);
        }
        free(r.text);
    } else if (a->job.kind == DS_JOB_BALANCE) {
        if (a->job.balance.ok) {
            snprintf(a->set_msg, sizeof a->set_msg, "key works - %s %s, available: %s",
                     a->job.balance.currency, a->job.balance.total,
                     a->job.balance.is_available ? "yes" : "no");
            app_state_set_status(a, "key check ok");
        } else {
            snprintf(a->set_msg, sizeof a->set_msg, "key check failed: %s",
                     a->job.balance.err);
            app_state_set_status(a, "key check failed");
        }
        memset(&a->job.balance, 0, sizeof a->job.balance);
    }

    ds_job_harvest(&a->job);
}
