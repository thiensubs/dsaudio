// app_state.h - the whole UI state, in plain C, owned by the frame loop.
#ifndef APP_STATE_H
#define APP_STATE_H

#include "../core/ds_asr.h"
#include "../core/ds_audio.h"
#include "../core/ds_config.h"
#include "../core/ds_job.h"
#include "../core/ds_log.h"
#include "../core/ds_player.h"
#include "../core/ds_tts.h"

#include <cui/cui.h>

enum { PAGE_TTS = 0, PAGE_ASR, PAGE_MCP, PAGE_SETTINGS };

typedef struct history_item {
    char stamp[32];
    char kind[8];        /* "tts" or "asr" */
    char detail[160];    /* voice/format, or the file name */
    char text[512];      /* source text or transcript preview */
    size_t bytes;
    int status;          /* 0 ok, 1 error */
    char note[160];
} history_item;

typedef struct app_state {
    ds_config cfg;
    ds_job job;
    ds_player *player;

    int page;

    /* TTS */
    char tts_text[16384];
    int tts_voice;       /* 0 stella, 1 tide */
    int tts_format;      /* 0 wav, 1 pcm */
    bool tts_filter;
    bool tts_preview;
    ds_tts_result tts;
    ds_tts_artifact artifact;
    char tts_path[512];
    float tts_peaks[256];
    int tts_peak_count;
    float tts_progress;
    double tts_duration_s;
    bool tts_dirty;      /* the text changed after the last generation */

    /* ASR */
    char asr_path[512];
    char asr_transcript[16384];
    char asr_original[16384];
    bool asr_dirty;
    int64_t asr_duration_ms;
    char asr_request_id[128];
    ds_audio_info asr_info;
    bool asr_info_ok;

    /* history */
    history_item history[64];
    int history_count;
    int history_sel;

    /* settings */
    char set_key[512];
    char set_base[256];
    char set_out[512];
    char set_msg[256];
    double set_tts_timeout, set_asr_timeout, set_history_limit, set_peaks;
    int set_language;
    bool set_dirty;

    /* shell */
    bool context_open;  /* the Session sheet */
    char status[256];
    int theme_mode;      /* 0 light, 1 dark, 2 system */
} app_state;

void app_state_init(app_state *a);
void app_state_free(app_state *a);

void app_state_set_status(app_state *a, const char *fmt, ...);
void app_state_add_history(app_state *a, const char *kind, const char *detail,
                           const char *text, size_t bytes, int status, const char *note);

/* Harvest a finished job into the UI state. Call once per frame. */
void app_state_poll_job(app_state *a);

/* Start a job from anywhere (a button or a shortcut). */
void app_state_request_tts(app_state *a);
void app_state_request_asr(app_state *a);

void ui_tts(cui_ctx *ui, app_state *a);
void ui_asr(cui_ctx *ui, app_state *a);
void ui_mcp(cui_ctx *ui, app_state *a);
void ui_settings(cui_ctx *ui, app_state *a);

#endif
