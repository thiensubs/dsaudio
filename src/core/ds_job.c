#include "ds_job.h"

#include "ds_audio.h"
#include "ds_tts_cache.h"
#include "ds_log.h"

#include <time.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void *job_thread(void *user) {
    ds_job *j = (ds_job *)user;
    double t0 = ds_now_sec();

    pthread_mutex_lock(&j->mutex);
    ds_job_kind kind = j->kind;
    ds_config cfg = j->cfg;
    ds_tts_options opt;
    memset(&opt, 0, sizeof opt);
    opt.text = j->text;
    opt.voice = j->voice;
    opt.format = j->format;
    opt.filter_markdown = j->filter_markdown;
    opt.timeout_s = j->timeout_s;
    char path[512];
    snprintf(path, sizeof path, "%s", j->path);
    pthread_mutex_unlock(&j->mutex);

    if (kind == DS_JOB_TTS) {
        /* Long text is synthesized in cached sentence chunks, so an edit only
         * re-synthesizes the chunks that changed. */
        ds_tts_chunk_result cr;
        if (!ds_tts_synth_cached(&cfg, j->text, j->voice, j->format, j->filter_markdown,
                                 &cr, NULL, NULL)) {
            ds_err_set(&j->tts.err, "%s", cr.err);
            ds_log_add(DS_LOG_ERROR, "tts: %s", j->tts.err.text);
            ds_tts_chunk_result_free(&cr);
        } else {
            ds_tts_result_free(&j->tts);
            j->tts.ok = true;
            j->tts.audio = cr.audio;
            snprintf(j->tts.content_type, sizeof j->tts.content_type, "%s", cr.content_type);
            j->artifact.chunks = cr.chunks;
            j->artifact.cached = cr.cached;
            j->artifact.generated = cr.generated;
            ds_tts_chunk_result_free(&cr);
            /* Heavy work stays here, on the worker: probe the audio, reduce it
             * to waveform peaks and write the file. */
            ds_tts_artifact *art = &j->artifact;
            memset(art, 0, sizeof *art);
            ds_audio_info info;
            if (ds_audio_probe(j->tts.audio.data, j->tts.audio.len, &info)) {
                int want = cfg.peaks > 0 ? cfg.peaks : DS_TTS_PEAKS;
                if (want > DS_TTS_PEAKS) want = DS_TTS_PEAKS;
                art->peak_count = ds_audio_peaks(j->tts.audio.data, j->tts.audio.len, &info,
                                                 art->peaks, want);
                art->duration_s = info.duration_s;
            }
            char err[256];
            if (ds_config_ensure_output_dir(&cfg, err, sizeof err)) {
                time_t now = time(NULL);
                struct tm tm;
                localtime_r(&now, &tm);
                char stamp[32];
                strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
                snprintf(art->path, sizeof art->path, "%s/tts-%s.%s", cfg.output_dir, stamp,
                         strcmp(opt.format, "pcm") == 0 ? "pcm" : "wav");
                FILE *f = fopen(art->path, "wb");
                if (f) {
                    fwrite(j->tts.audio.data, 1, j->tts.audio.len, f);
                    fclose(f);
                    ds_log_add(DS_LOG_OK, "wrote %s (%zu bytes)", art->path, j->tts.audio.len);
                } else {
                    ds_log_add(DS_LOG_ERROR, "cannot write %s", art->path);
                }
            } else {
                ds_log_add(DS_LOG_WARN, "%s", err);
            }
            ds_log_add(DS_LOG_OK, "tts: %zu bytes, %s (%d chunks, %d cached, %d synthesized)",
                       j->tts.audio.len, j->tts.content_type, j->artifact.chunks,
                       j->artifact.cached, j->artifact.generated);
        }
    } else if (kind == DS_JOB_ASR) {
        if (!ds_asr_run(&cfg, path, j->timeout_s, &j->asr))
            ds_log_add(DS_LOG_ERROR, "asr: %s", j->asr.err.text);
        else
            ds_log_add(DS_LOG_OK, "asr: %lld ms, %zu chars",
                       (long long)j->asr.duration_ms, strlen(j->asr.text ? j->asr.text : ""));
    } else if (kind == DS_JOB_BALANCE) {
        if (!ds_balance_query(&cfg, 20, &j->balance))
            ds_log_add(DS_LOG_ERROR, "key check: %s", j->balance.err);
        else
            ds_log_add(DS_LOG_OK, "key ok: %s %s available=%s",
                       j->balance.currency, j->balance.total,
                       j->balance.is_available ? "yes" : "no");
    }

    pthread_mutex_lock(&j->mutex);
    j->elapsed = ds_now_sec() - t0;
    j->done = true;
    pthread_mutex_unlock(&j->mutex);
    return NULL;
}

void ds_job_init(ds_job *j) {
    memset(j, 0, sizeof *j);
    pthread_mutex_init(&j->mutex, NULL);
}

void ds_job_shutdown(ds_job *j) {
    if (!j) return;
    if (j->running) pthread_join(j->thread, NULL);
    ds_tts_result_free(&j->tts);
    ds_asr_result_free(&j->asr);
    pthread_mutex_destroy(&j->mutex);
}

static bool start(ds_job *j) {
    if (j->running) return false;
    j->done = false;
    j->elapsed = 0.0;
    ds_tts_result_free(&j->tts);
    ds_asr_result_free(&j->asr);
    memset(&j->artifact, 0, sizeof j->artifact);
    if (pthread_create(&j->thread, NULL, job_thread, j) != 0) return false;
    j->running = true;
    return true;
}

bool ds_job_start_tts(ds_job *j, const ds_config *cfg, const char *text,
                      const char *voice, const char *format, bool filter_markdown) {
    pthread_mutex_lock(&j->mutex);
    j->kind = DS_JOB_TTS;
    j->cfg = *cfg;
    snprintf(j->text, sizeof j->text, "%s", text ? text : "");
    snprintf(j->voice, sizeof j->voice, "%s", voice ? voice : "stella");
    snprintf(j->format, sizeof j->format, "%s", format ? format : "wav");
    j->filter_markdown = filter_markdown;
    j->timeout_s = cfg->tts_timeout_s > 0 ? cfg->tts_timeout_s : 300;
    pthread_mutex_unlock(&j->mutex);
    return start(j);
}

bool ds_job_start_asr(ds_job *j, const ds_config *cfg, const char *path) {
    pthread_mutex_lock(&j->mutex);
    j->kind = DS_JOB_ASR;
    j->cfg = *cfg;
    snprintf(j->path, sizeof j->path, "%s", path ? path : "");
    j->timeout_s = cfg->asr_timeout_s > 0 ? cfg->asr_timeout_s : 600;
    pthread_mutex_unlock(&j->mutex);
    return start(j);
}

bool ds_job_start_balance(ds_job *j, const ds_config *cfg) {
    pthread_mutex_lock(&j->mutex);
    j->kind = DS_JOB_BALANCE;
    j->cfg = *cfg;
    j->timeout_s = 20;
    pthread_mutex_unlock(&j->mutex);
    return start(j);
}

bool ds_job_is_running(const ds_job *j) {
    /* True until the result is harvested: a finished job still owns its slot,
     * and its output must not be cleared by a new job before the UI reads it. */
    pthread_mutex_lock((pthread_mutex_t *)&j->mutex);
    bool r = j->running;
    pthread_mutex_unlock((pthread_mutex_t *)&j->mutex);
    return r;
}

bool ds_job_poll(ds_job *j) {
    pthread_mutex_lock(&j->mutex);
    bool done = j->done;
    pthread_mutex_unlock(&j->mutex);
    return done;
}

void ds_job_harvest(ds_job *j) {
    pthread_mutex_lock(&j->mutex);
    bool r = j->running;
    pthread_mutex_unlock(&j->mutex);
    if (r) pthread_join(j->thread, NULL);
    pthread_mutex_lock(&j->mutex);
    j->running = false;
    j->done = false;   /* cleared here, so poll cannot report the same job twice */
    pthread_mutex_unlock(&j->mutex);
}
