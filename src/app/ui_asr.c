// ui_asr.c - the speech to text workbench.
#include "app_state.h"

#include "../core/ds_i18n.h"

#include <cui/cui.h>
#include <cui/cui_app.h>

#include <stdio.h>
#include <string.h>

static void pick_file(cui_ctx *ui, app_state *a) {
    char path[600];
    cui_file_dialog_opts opts;
    memset(&opts, 0, sizeof opts);
    opts.kind = CUI_FILE_OPEN;
    opts.title = ds_i18n("asr.choose");
    opts.filter_name = ds_i18n("asr.audio");
    opts.filter = "wav;ogg;opus";
    if (cui_app_file_dialog(NULL, &opts, path, sizeof path)) {
        snprintf(a->asr_path, sizeof a->asr_path, "%s", path);
        a->asr_info_ok = ds_audio_probe_file(a->asr_path, &a->asr_info);
        a->asr_transcript[0] = '\0';
        a->asr_original[0] = '\0';
        a->asr_dirty = false;
        a->asr_duration_ms = 0;
        a->asr_request_id[0] = '\0';
        app_state_set_status(a, "loaded %s", a->asr_path);
    }
}

static void file_card(cui_ctx *ui, app_state *a) {
    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_row(ui, .gap = 8) {
            cui_heading(ui, ds_i18n("asr.audio"), 4);
            cui_spacer(ui);
            if (a->asr_path[0])
                cui_badge(ui, ds_i18n("asr.loaded"), .tone = CUI_TONE_INFO, .dot = true);
        }

        int drop = cui_dropzone(ui, "asr-drop",
                                .title = ds_i18n("asr.drop_title"),
                                .description = ds_i18n("asr.drop_desc"),
                                .accept = "wav;ogg;opus;mp3;flac",
                                .height = 140,
                                .width = CUI_FILL);
        if (drop == -1) pick_file(ui, a);
        for (int i = 0; i < drop && i < 4; i++) {
            snprintf(a->asr_path, sizeof a->asr_path, "%s", cui_dropped_file(ui, i));
            a->asr_info_ok = ds_audio_probe_file(a->asr_path, &a->asr_info);
            app_state_set_status(a, "loaded %s", a->asr_path);
            break;
        }

        if (a->asr_path[0]) {
            char line[512];
            if (a->asr_info_ok) {
                snprintf(line, sizeof line, "%s - %d Hz, %d ch, %.1f s",
                         a->asr_path, a->asr_info.sample_rate, a->asr_info.channels,
                         a->asr_info.duration_s);
            } else {
                snprintf(line, sizeof line, "%s", a->asr_path);
            }
            cui_text(ui, line, .nowrap = true);
            if (a->asr_info_ok) {
                char meta[128];
                snprintf(meta, sizeof meta, "%.1f s of speech", a->asr_info.duration_s);
                cui_secondary(ui, meta);
            }
        }

        if (a->job.asr.err.text[0] && !a->job.asr.ok) {
            cui_alert(ui, ds_i18n("asr.fail_title"), .description = a->job.asr.err.text,
                      .tone = CUI_TONE_DANGER, .dismissible = true);
            cui_copy_button(ui, "copy-asr-err", a->job.asr.err.text);
        }

        cui_alert(ui, ds_i18n("asr.convert"),
                  .description = ds_i18n("asr.convert_desc"),
                  .tone = CUI_TONE_NEUTRAL);

        cui_alert(ui, ds_i18n("asr.lang_note"),
                  .description = ds_i18n("asr.lang_desc"),
                  .tone = CUI_TONE_NEUTRAL);

        cui_row(ui, .gap = 8) {
            if (cui_button(ui, ds_i18n("common.browse"), .icon = CUI_ICON_FOLDER_OPEN)) pick_file(ui, a);
            if (cui_button(ui, ds_job_is_running(&a->job) ? ds_i18n("asr.transcribing") : ds_i18n("asr.transcribe"),
                           .variant = CUI_BUTTON_PRIMARY,
                           .icon = CUI_ICON_MIC,
                           .loading = ds_job_is_running(&a->job),
                           .disabled = !a->asr_path[0]))
                app_state_request_asr(a);
            cui_tooltip(ui, "Ctrl T");
        }
    }
}

static void transcript_card(cui_ctx *ui, app_state *a) {
    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_row(ui, .gap = 8) {
            cui_heading(ui, ds_i18n("asr.transcript"), 4);
            cui_spacer(ui);
            if (a->asr_dirty)
                cui_badge(ui, ds_i18n("common.edited"), .tone = CUI_TONE_WARNING, .dot = true);
            else if (a->asr_transcript[0])
                cui_badge(ui, ds_i18n("asr.automatic"), .tone = CUI_TONE_SUCCESS, .dot = true);
        }

        if (!a->asr_transcript[0]) {
            cui_empty_state(ui, ds_i18n("asr.no_transcript"),
                            .icon = CUI_ICON_FILE_TEXT,
                            .description = ds_i18n("asr.no_transcript_desc"),
                            .action = NULL);
            return;
        }

        if (a->asr_duration_ms > 0) {
            char meta[96];
            snprintf(meta, sizeof meta, "%lld ms - request %s",
                     (long long)a->asr_duration_ms,
                     a->asr_request_id[0] ? a->asr_request_id : "-");
            cui_secondary(ui, meta);
        }

        cui_code_editor_result r = cui_code_editor(ui, "asr-text", a->asr_transcript,
                                                   sizeof a->asr_transcript, .lang = "text",
                                                   .height = 200,
                                                   .placeholder = ds_i18n("asr.placeholder"));
        if (r.changed) a->asr_dirty = true;
        if (r.refused)
            cui_toast(ui, ds_i18n("common.edit_too_long"), .tone = CUI_TONE_WARNING);

        if (a->asr_dirty && strcmp(a->asr_original, a->asr_transcript) != 0) {
            cui_separator(ui);
            cui_row(ui, .gap = 8) {
                cui_text(ui, ds_i18n("asr.correction"), .font = CUI_FONT_MEDIUM);
                cui_spacer(ui);
                if (cui_button(ui, ds_i18n("asr.revert"), .icon = CUI_ICON_REFRESH_CW)) {
                    snprintf(a->asr_transcript, sizeof a->asr_transcript, "%s", a->asr_original);
                    a->asr_dirty = false;
                }
            }
            int change = 0;
            cui_diff_result d = cui_diff_view(ui, "asr-diff", a->asr_original, a->asr_transcript,
                                              .mode = CUI_DIFF_SPLIT, .current = &change,
                                              .language = "text", .old_title = ds_i18n("asr.automatic"),
                                              .new_title = ds_i18n("asr.corrected"), .height = 220);
            cui_textf(ui, "+%d -%d lines", d.added, d.removed);
        }

        cui_row(ui, .gap = 8) {
            cui_copy_button(ui, "copy-transcript", a->asr_transcript);
            if (cui_button(ui, ds_i18n("asr.save"), .icon = CUI_ICON_DOWNLOAD)) {
                char path[600];
                cui_file_dialog_opts opts;
                memset(&opts, 0, sizeof opts);
                opts.kind = CUI_FILE_SAVE;
                opts.title = ds_i18n("asr.save_title");
                opts.default_name = "transcript.txt";
                if (cui_app_file_dialog(NULL, &opts, path, sizeof path)) {
                    FILE *f = fopen(path, "wb");
                    if (f) {
                        fwrite(a->asr_transcript, 1, strlen(a->asr_transcript), f);
                        fclose(f);
                        ds_log_add(DS_LOG_OK, "wrote %s", path);
                        app_state_set_status(a, ds_i18n("asr.saved"), path);
                    } else {
                        app_state_set_status(a, ds_i18n("asr.save_failed"), path);
                    }
                }
            }
            cui_spacer(ui);
            char cc[64];
            snprintf(cc, sizeof cc, "%zu characters", ds_utf8_len(a->asr_transcript));
            cui_text(ui, cc, .color = CUI_COLOR_TEXT_SECONDARY);
        }
    }
}

void ui_asr(cui_ctx *ui, app_state *a) {
    cui_row(ui, .gap = 8) {
        cui_heading(ui, ds_i18n("asr.title"), 2);
        cui_spacer(ui);
        cui_badge(ui, a->status, .tone = CUI_TONE_NEUTRAL);
    }
    cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER) {
        cui_text(ui, "DeepSeek", .color = CUI_COLOR_TEXT_SECONDARY);
        cui_code(ui, "/v1/tools/asr");
        cui_text(ui, "POST", .font = CUI_FONT_MONO, .size = 11,
                 .color = CUI_COLOR_TEXT_SECONDARY);
    }
    cui_secondary(ui, ds_i18n("asr.subtitle"));

    file_card(ui, a);
    transcript_card(ui, a);
}
