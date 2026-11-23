// ui_settings.c - connection, output location and the MCP install snippet.
#include "app_state.h"

#include <cui/cui.h>
#include <cui/cui_app.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ds_i18n.h"

static const char *kDefVoices[] = { "Stella", "Tide" };
static const char *kDefFormats[] = { "WAV", "PCM" };
/* endonyms: each language names itself */
static const char *kLangs[] = { "English", "Ti\u1ebfng Vi\u1ec7t", "\u4e2d\u6587",
                                 "\u65e5\u672c\u8a9e", "\ud55c\uad6d\uae00",
                                 "Espa\u00f1ol", "Fran\u00e7ais", "Deutsch" };

static void limits_card(cui_ctx *ui, app_state *a) {
    (void)a;
    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, "Documented limits", 4);
        cui_row(ui, .gap = 16) {
            cui_column(ui, .gap = 10) {
                cui_readout(ui, "TTS TEXT", "10 000", .unit = "BILLED", .size = CUI_SIZE_SM);
                cui_readout(ui, "TTS BODY", "64", .unit = "KiB", .size = CUI_SIZE_SM);
                cui_readout(ui, "TTS LENGTH", "30", .unit = "MIN", .size = CUI_SIZE_SM);
            }
            cui_column(ui, .gap = 10) {
                cui_readout(ui, "ASR FILE", "8", .unit = "MiB", .size = CUI_SIZE_SM);
                cui_readout(ui, "ASR LENGTH", "120", .unit = "SEC", .size = CUI_SIZE_SM);
                cui_readout(ui, "ASR UPLOAD", "10", .unit = "MIN", .size = CUI_SIZE_SM);
            }
        }
        cui_separator(ui);
        cui_secondary(ui, "A CJK character counts as two billed characters. A response that "
                          "ends early is treated as a failure, not as short audio.");
    }
}

void ui_settings(cui_ctx *ui, app_state *a) {
    cui_row(ui, .gap = 8) {
        cui_heading(ui, "Settings", 2);
        cui_spacer(ui);
        cui_copy_button(ui, "copy-source", a->cfg.source);
    }
    cui_secondary(ui, "The key is read from $DEEPSEEK_API_KEY, then this app's own config file, "
                      "then the DeepSeek provider block in kimi-code's config.");

    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_row(ui, .gap = 8) {
            if (cui_input(ui, "api-key", a->set_key, sizeof a->set_key,
                          .label = "DeepSeek API key",
                          .placeholder = "sk-...",
                          .password = true,
                          .clearable = true,
                          .width = CUI_PX(420),
                          .hint = "Stored in the OS keychain when it is available. Never printed."))
                a->set_dirty = true;
            if (cui_button(ui, ds_job_is_running(&a->job) ? "Checking..." : "Check key",
                           .icon = CUI_ICON_KEY_ROUND,
                           .loading = ds_job_is_running(&a->job))) {
                a->cfg.have_key = a->set_key[0] != '\0';
                snprintf(a->cfg.api_key, sizeof a->cfg.api_key, "%s", a->set_key);
                snprintf(a->cfg.base_url, sizeof a->cfg.base_url, "%s", a->set_base);
                ds_job_start_balance(&a->job, &a->cfg);
            }
        }

        if (cui_input(ui, "base-url", a->set_base, sizeof a->set_base,
                      .label = "Base URL",
                      .placeholder = "https://api.deepseek.com"))
            a->set_dirty = true;

        cui_row(ui, .gap = 8) {
            if (cui_input(ui, "out-dir", a->set_out, sizeof a->set_out,
                          .label = "Output folder",
                          .width = CUI_PX(560),
                          .placeholder = "~/.dsaudio/output"))
                a->set_dirty = true;
            if (cui_button(ui, "Browse...", .icon = CUI_ICON_FOLDER_OPEN)) {
                char path[600];
                cui_file_dialog_opts opts;
                memset(&opts, 0, sizeof opts);
                opts.kind = CUI_FILE_FOLDER;
                opts.title = "Choose the output folder";
                if (cui_app_file_dialog(NULL, &opts, path, sizeof path)) {
                    snprintf(a->set_out, sizeof a->set_out, "%s", path);
                    a->set_dirty = true;
                }
            }
        }

        if (a->set_msg[0]) {
            cui_alert(ui, a->set_msg,
                      .tone = strstr(a->set_msg, "works") ? CUI_TONE_SUCCESS : CUI_TONE_DANGER);
            cui_copy_button(ui, "copy-set-msg", a->set_msg);
        }

        cui_row(ui, .gap = 8) {
            if (cui_button(ui, "Save settings", .variant = CUI_BUTTON_PRIMARY,
                           .icon = CUI_ICON_CHECK, .disabled = !a->set_dirty)) {
                ds_config save = a->cfg;
                snprintf(save.api_key, sizeof save.api_key, "%s", a->set_key);
                snprintf(save.base_url, sizeof save.base_url, "%s", a->set_base);
                snprintf(save.output_dir, sizeof save.output_dir, "%s", a->set_out);
                save.tts_timeout_s = (int)(a->set_tts_timeout + 0.5);
                save.asr_timeout_s = (int)(a->set_asr_timeout + 0.5);
                save.history_limit = (int)(a->set_history_limit + 0.5);
                save.peaks = (int)(a->set_peaks + 0.5);
                snprintf(save.default_voice, sizeof save.default_voice, "%s",
                         a->tts_voice == 1 ? "tide" : "stella");
                snprintf(save.default_format, sizeof save.default_format, "%s",
                         a->tts_format == 1 ? "pcm" : "wav");
                save.filter_markdown = a->tts_filter;
                save.theme = a->theme_mode;
                snprintf(save.language, sizeof save.language, "%s", ds_language_code(a->set_language));
                char err[300];
                bool key_ok = true;
                bool saved = ds_config_save_ex(&save, &key_ok, err, sizeof err);
                if (saved) {
                    a->cfg = save;
                    a->set_dirty = false;
                    snprintf(a->set_msg, sizeof a->set_msg, "saved to %s", err);
                    ds_log_add(DS_LOG_OK, "settings saved to %s", err);
                } else {
                    snprintf(a->set_msg, sizeof a->set_msg, "%s", err);
                    ds_log_add(DS_LOG_WARN, "%s", err);
                }
            }
            if (cui_button(ui, "Reload from disk", .icon = CUI_ICON_REFRESH_CW)) {
                ds_config_load(&a->cfg);
                snprintf(a->set_key, sizeof a->set_key, "%s", a->cfg.api_key);
                snprintf(a->set_base, sizeof a->set_base, "%s", a->cfg.base_url);
                snprintf(a->set_out, sizeof a->set_out, "%s", a->cfg.output_dir);
                a->set_dirty = false;
                snprintf(a->set_msg, sizeof a->set_msg, "reloaded");
            }
        }

        char masked[128];
        ds_config_mask_key(&a->cfg, masked, sizeof masked);
        cui_separator(ui);
        cui_heading(ui, "Defaults and limits", 4);
        cui_row(ui, .gap = 12, .wrap = true) {
            cui_segmented(ui, "def-voice", kDefVoices, 2, &a->tts_voice);
            cui_segmented(ui, "def-format", kDefFormats, 2, &a->tts_format);
            cui_switch(ui, "Filter Markdown by default", &a->tts_filter);
        }
        cui_row(ui, .gap = 12, .wrap = true) {
            if (cui_number(ui, "tts-timeout", &a->set_tts_timeout, 30, 900,
                           .label = "TTS timeout", .suffix = "s", .step = 30))
                a->set_dirty = true;
            if (cui_number(ui, "asr-timeout", &a->set_asr_timeout, 60, 1800,
                           .label = "ASR timeout", .suffix = "s", .step = 60))
                a->set_dirty = true;
            if (cui_number(ui, "history-limit", &a->set_history_limit, 8, 512,
                           .label = "History entries", .step = 8))
                a->set_dirty = true;
            if (cui_number(ui, "peaks", &a->set_peaks, 32, 1024,
                           .label = "Waveform bars", .step = 32))
                a->set_dirty = true;
        }
        cui_row(ui, .gap = 12, .wrap = true) {
            if (cui_select(ui, "language", kLangs, 8, &a->set_language,
                           .label = "Interface language", .width = CUI_PX(240)))
                a->set_dirty = true;
        }
        cui_row(ui, .gap = 8) {
            cui_text(ui, "Theme", .font = CUI_FONT_MEDIUM, .width = CUI_PX(120));
            cui_theme_switcher(ui, &a->theme_mode, true);
        }

        char inforce[700];
        snprintf(inforce, sizeof inforce, "In force now: %s - %s - base %s - output %s",
                 masked, a->cfg.key_store[0] ? a->cfg.key_store : "-",
                 a->cfg.base_url, a->cfg.output_dir);
        cui_text(ui, inforce, .color = CUI_COLOR_TEXT_SECONDARY);
    }

    limits_card(ui, a);
}
