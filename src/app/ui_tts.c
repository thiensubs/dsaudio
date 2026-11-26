// ui_tts.c - the text to speech workbench.
#include "app_state.h"

#include "../core/ds_i18n.h"

#include <cui/cui.h>
#include <cui/cui_app.h>

#include <stdio.h>
#include <stdlib.h>

#include "../core/ds_tts_cache.h"
#include <string.h>

static const char *kVoices[] = {"Stella", "Tide"};
static const char *kVoiceIds[] = {"stella", "tide"};
static const char *kFormats[] = {"WAV", "PCM"};

static void result_card(cui_ctx *ui, app_state *a) {
    cui_card(ui, .id = "tts-result-card", .width = CUI_FILL, .gap = 12) {
        cui_row(ui, .gap = 8) {
            cui_heading(ui, ds_i18n("tts.result"), 4);
            cui_spacer(ui);
            if (a->tts.ok) {
                cui_badge(ui, ds_i18n("tts.ready"), .tone = CUI_TONE_SUCCESS, .dot = true);
            } else if (a->tts.err.text[0]) {
                cui_badge(ui, ds_i18n("common.failed"), .tone = CUI_TONE_DANGER, .dot = true);
            } else {
                cui_badge(ui, ds_i18n("tts.empty"), .tone = CUI_TONE_NEUTRAL, .dot = true);
            }
        }

        if (a->tts.err.text[0] && !a->tts.ok) {
            cui_alert(ui, ds_i18n("tts.fail_title"), .description = a->tts.err.text,
                      .tone = CUI_TONE_DANGER, .dismissible = true);
            cui_copy_button(ui, "copy-tts-err", a->tts.err.text);
        }

        if (!a->tts.ok) {
            if (cui_empty_state(ui, ds_i18n("tts.no_audio"),
                                .icon = CUI_ICON_AUDIO_LINES,
                                .description = ds_i18n("tts.no_audio_desc"),
                                .action = ds_i18n("tts.generate")))
                app_state_request_tts(a);
            return;
        }

        cui_now_playing np;
        memset(&np, 0, sizeof np);
        np.title = ds_i18n("tts.clip");
        char artist[64];
        double dur = a->tts_duration_s > 0.0 ? a->tts_duration_s : ds_player_duration(a->player);
        snprintf(artist, sizeof artist, "%s / %s - %.1fs",
                 kVoiceIds[a->tts_voice], a->tts_format == 1 ? "pcm" : "wav", dur);
        np.artist = artist;
        np.art_color = CUI_COLOR_ACCENT;
        np.duration = dur;
        np.position = ds_player_position(a->player);
        np.playing = ds_player_is_playing(a->player);
        int ev = cui_now_playing(ui, "tts-player", &np, .width = CUI_FILL);
        if (ev & CUI_PLAYER_TOGGLE) {
            if (ds_player_is_playing(a->player)) {
                ds_player_pause(a->player);
            } else {
                if (!ds_player_is_open(a->player) && a->tts_path[0]) {
                    char perr[256];
                    if (!ds_player_open(a->player, a->tts_path, perr, sizeof perr))
                        cui_toast(ui, perr, .tone = CUI_TONE_DANGER);
                }
                ds_player_play(a->player);
            }
        }
        if (ev & CUI_PLAYER_SEEK) ds_player_seek(a->player, (double)np.position);

        if (a->tts_peak_count > 0) {
            if (cui_waveform(ui, "tts-wave", a->tts_peaks, a->tts_peak_count,
                             &a->tts_progress, .height = 56, .width = CUI_FILL))
                ds_player_seek(a->player, (double)a->tts_progress * dur);
        }

        char meta[320];
        snprintf(meta, sizeof meta, "%zu bytes - %s - stored in %s", a->tts.audio.len,
                 a->tts.content_type, a->tts_path);
        cui_secondary(ui, meta);
        if (a->artifact.chunks > 0) {
            char chunks[160];
            snprintf(chunks, sizeof chunks, "%d chunks - %d from cache, %d synthesized",
                     a->artifact.chunks, a->artifact.cached, a->artifact.generated);
            cui_text(ui, chunks, .font = CUI_FONT_MONO, .size = 11,
                     .color = CUI_COLOR_TEXT_SECONDARY);
        }

        cui_row(ui, .gap = 8) {
            if (ds_player_is_playing(a->player)) {
                if (cui_button(ui, ds_i18n("tts.pause"), .icon = CUI_ICON_PAUSE)) ds_player_pause(a->player);
            } else {
                if (cui_button(ui, ds_i18n("tts.play"), .variant = CUI_BUTTON_PRIMARY,
                               .icon = CUI_ICON_PLAY)) {
                    if (!ds_player_is_open(a->player) && a->tts_path[0]) {
                        char perr[256];
                        if (!ds_player_open(a->player, a->tts_path, perr, sizeof perr))
                            cui_toast(ui, perr, .tone = CUI_TONE_DANGER);
                    }
                    ds_player_play(a->player);
                }
            }
            if (cui_button(ui, ds_i18n("tts.open_folder"), .icon = CUI_ICON_FOLDER_OPEN)) {
                char url[900];
                snprintf(url, sizeof url, "file://%s", a->cfg.output_dir);
                if (!cui_app_open_url(url))
                    cui_toast(ui, ds_i18n("tts.open_folder_fail"), .tone = CUI_TONE_WARNING);
            }
            if (cui_button(ui, ds_i18n("tts.clear_cache"), .icon = CUI_ICON_TRASH_2)) {
                char cerr[256];
                if (ds_cache_clear(cerr, sizeof cerr)) {
                    snprintf(a->status, sizeof a->status, "%s", ds_i18n("tts.cache_cleared"));
                    ds_log_add(DS_LOG_OK, "chunk cache cleared");
                } else {
                    snprintf(a->status, sizeof a->status, "%s", cerr);
                }
            }
            cui_spacer(ui);
            cui_copy_button(ui, "copy-tts-path", a->tts_path);
        }
    }
}

static void history_card(cui_ctx *ui, app_state *a) {
    cui_card(ui, .id = "tts-history-card", .width = CUI_FILL, .gap = 10) {
        cui_heading(ui, ds_i18n("tts.history"), 4);
        if (a->history_count == 0) {
            cui_secondary(ui, ds_i18n("tts.history_none"));
            return;
        }
        cui_grid_column cols[] = {
            { ds_i18n("tts.col_time"), 76, CUI_TEXT_LEFT, false, false, 0 },
            { ds_i18n("tts.col_kind"), 52, CUI_TEXT_LEFT, false, false, 0 },
            { ds_i18n("tts.col_detail"), 150, CUI_TEXT_LEFT, false, false, 0 },
            { ds_i18n("tts.col_size"), 80, CUI_TEXT_RIGHT, false, false, 0 },
            { ds_i18n("tts.col_text"), 0, CUI_TEXT_LEFT, false, false, 0 },
            { ds_i18n("tts.col_status"), 70, CUI_TEXT_LEFT, false, false, 0 },
        };
        int first = 0, last = 0;
        if (cui_data_grid_begin(ui, "tts-history", cols, 6, a->history_count,
                                &first, &last, .height = CUI_PX(220), .striped = true)) {
            for (int r = first; r < last; r++) {
                if (cui_data_grid_row(ui, r)) a->history_sel = r;
                history_item *h = &a->history[r];
                cui_data_grid_cell_text(ui, h->stamp);
                cui_data_grid_cell_text(ui, h->kind);
                cui_data_grid_cell_text(ui, h->detail);
                char size[32];
                snprintf(size, sizeof size, "%zu", h->bytes);
                cui_data_grid_cell_text(ui, size);
                cui_data_grid_cell_text(ui, h->text);
                cui_data_grid_cell_text(ui, h->status == 0 ? ds_i18n("common.ok") : ds_i18n("common.failed"));
            }
            cui_data_grid_end(ui);
        }
    }
}

void ui_tts(cui_ctx *ui, app_state *a) {
    size_t chars = ds_utf8_len(a->tts_text);
    size_t billed = ds_utf8_billed_len(a->tts_text);
    bool over = billed > DS_TTS_MAX_BILLED;
    bool running = ds_job_is_running(&a->job);

    cui_row(ui, .gap = 8) {
        cui_heading(ui, ds_i18n("tts.title"), 2);
        cui_spacer(ui);
        if (a->tts_dirty && a->tts.ok)
            cui_badge(ui, ds_i18n("tts.edited"), .tone = CUI_TONE_WARNING, .dot = true);
        cui_badge(ui, a->status, .tone = CUI_TONE_NEUTRAL);
    }
    cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER) {
        cui_text(ui, "DeepSeek", .color = CUI_COLOR_TEXT_SECONDARY);
        cui_code(ui, "/v1/tools/tts");
        cui_text(ui, "POST", .font = CUI_FONT_MONO, .size = 11,
                 .color = CUI_COLOR_TEXT_SECONDARY);
    }
    cui_secondary(ui, ds_i18n("tts.subtitle"));

    cui_card(ui, .id = "tts-editor-card", .width = CUI_FILL, .gap = 12) {
        cui_row(ui, .gap = 12, .wrap = true) {
            cui_segmented(ui, "voice", kVoices, 2, &a->tts_voice);
            cui_segmented(ui, "format", kFormats, 2, &a->tts_format);
            cui_switch(ui, ds_i18n("tts.filter"), &a->tts_filter);
            cui_switch(ui, ds_i18n("tts.preview"), &a->tts_preview);
        }

        if (a->tts_preview) {
            cui_markdown(ui, "tts-preview", a->tts_text);
            cui_separator(ui);
        }

        cui_code_editor_result r = cui_code_editor(ui, "tts-text", a->tts_text,
                                                   sizeof a->tts_text, .lang = "text",
                                                   .height = 220,
                                                   .placeholder = ds_i18n("tts.placeholder"));
        if (r.changed && a->tts.ok) a->tts_dirty = true;
        if (r.refused)
            cui_toast(ui, ds_i18n("common.edit_too_long"), .tone = CUI_TONE_WARNING);

        cui_row(ui, .gap = 8) {
            char cnt[128];
            snprintf(cnt, sizeof cnt, "%zu characters - %zu billed - limit %d",
                     chars, billed, DS_TTS_MAX_BILLED);
            cui_text(ui, cnt, .color = over ? CUI_COLOR_DANGER : CUI_COLOR_TEXT_SECONDARY);
            cui_spacer(ui);
            if (cui_button(ui, ds_i18n("tts.clear"), .icon = CUI_ICON_TRASH_2)) {
                a->tts_text[0] = '\0';
                a->tts_dirty = false;
            }
            if (cui_button(ui, running ? ds_i18n("tts.generating") : ds_i18n("tts.generate"),
                           .variant = CUI_BUTTON_PRIMARY,
                           .icon = CUI_ICON_SPARKLES,
                           .loading = running,
                           .disabled = over || chars == 0))
                app_state_request_tts(a);
            cui_tooltip(ui, "Ctrl G");
        }
        if (over)
            cui_alert(ui, ds_i18n("tts.over_limit"),
                      .description = ds_i18n("tts.over_limit_desc"),
                      .tone = CUI_TONE_WARNING);
    }

    result_card(ui, a);
    history_card(ui, a);
}
