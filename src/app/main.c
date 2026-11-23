// main.c - the dsaudio workbench shell.
#include "app_state.h"

#include "../core/ds_http.h"

#include <cui/cui.h>
#include <cui/cui_app.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "fonts_cjk.h"

static const char *kPages[] = {"Speech", "Transcribe", "MCP", "Settings"};

static void nav(cui_ctx *ui, app_state *a) {
    cui_column(ui, .width = CUI_PX(200), .height = CUI_FILL, .pad = 14, .gap = 4,
               .bg = CUI_COLOR_SURFACE) {
        cui_row(ui, .gap = 8) {
            cui_icon(ui, CUI_ICON_AUDIO_LINES, 18, CUI_COLOR_ACCENT);
            cui_text(ui, "dsaudio", .font = CUI_FONT_MEDIUM);
        }
        cui_gap(ui, 10);

        if (cui_nav_item(ui, kPages[0], .icon = CUI_ICON_SPARKLES, .selected = a->page == PAGE_TTS))
            a->page = PAGE_TTS;
        if (cui_nav_item(ui, kPages[1], .icon = CUI_ICON_MIC, .selected = a->page == PAGE_ASR))
            a->page = PAGE_ASR;
        if (cui_nav_item(ui, kPages[2], .icon = CUI_ICON_SERVER, .selected = a->page == PAGE_MCP))
            a->page = PAGE_MCP;
        if (cui_nav_item(ui, kPages[3], .icon = CUI_ICON_SETTINGS, .selected = a->page == PAGE_SETTINGS))
            a->page = PAGE_SETTINGS;

        cui_spacer(ui);
        cui_separator(ui);
        cui_gap(ui, 8);
        if (cui_button(ui, "Session", .icon = CUI_ICON_LIST, .full_width = true))
            a->context_open = !a->context_open;
        cui_gap(ui, 6);
        cui_theme_switcher(ui, &a->theme_mode, true);
        cui_gap(ui, 6);
        char ver[64];
        snprintf(ver, sizeof ver, "C99 - CUI %s", "1.4.0");
        cui_muted(ui, ver);
    }
}

static void session_card(cui_ctx *ui, app_state *a) {
    cui_card(ui, .width = CUI_FILL, .gap = 10) {
        cui_heading(ui, "Last job", 4);
        if (a->history_count == 0) {
            cui_secondary(ui, "No job yet. The last TTS or ASR result is summarised here.");
            return;
        }
        int sel = a->history_sel;
        if (sel < 0 || sel >= a->history_count) sel = a->history_count - 1;
        history_item *h = &a->history[sel];
        cui_row(ui, .gap = 8) {
            cui_badge(ui, h->kind, .tone = h->status == 0 ? CUI_TONE_SUCCESS : CUI_TONE_DANGER);
            cui_text(ui, h->stamp, .font = CUI_FONT_MONO, .color = CUI_COLOR_TEXT_SECONDARY);
        }
        cui_text(ui, h->detail, .nowrap = true);
        cui_text(ui, h->text, .color = CUI_COLOR_TEXT_SECONDARY);
        cui_separator(ui);
        char meta[128];
        snprintf(meta, sizeof meta, "%zu bytes - %s", h->bytes, h->note);
        cui_text(ui, meta, .color = CUI_COLOR_TEXT_SECONDARY);
        if (cui_button(ui, "Clear history", .icon = CUI_ICON_TRASH_2)) {
            a->history_count = 0;
            a->history_sel = -1;
        }
    }
}

static void activity_card(cui_ctx *ui, app_state *a) {
    (void)a;
    cui_card(ui, .width = CUI_FILL, .gap = 8) {
        cui_heading(ui, "Activity", 4);
        int n = ds_log_count();
        if (n == 0) {
            cui_secondary(ui, "Nothing logged yet.");
            return;
        }
        int start = n > 12 ? n - 12 : 0;
        for (int i = start; i < n; i++) {
            const ds_log_entry *e = ds_log_at(i);
            if (!e) continue;
            cui_text(ui, e->text, .font = CUI_FONT_MONO, .size = 11,
                     .color = e->level == DS_LOG_ERROR ? CUI_COLOR_DANGER
                           : e->level == DS_LOG_OK ? CUI_COLOR_SUCCESS
                           : CUI_COLOR_TEXT_SECONDARY);
        }
    }
}

static void right_panel(cui_ctx *ui, app_state *a) {
    cui_column(ui, .width = CUI_PX(300), .height = CUI_FILL, .pad = 16, .gap = 14,
               .bg = CUI_COLOR_SURFACE, .scroll_y = true) {
        session_card(ui, a);
        activity_card(ui, a);
        cui_secondary(ui, "This window renders continuously so the waveform and progress stay "
                          "live, which uses CPU while idle.");
    }
}

/* The embedded Inter covers Latin + Vietnamese only. For the CJK languages we
 * use Noto subsets generated from the exact glyphs ds_i18n.c uses, chained as
 * fallbacks so one pass covers Chinese, Japanese and Korean. */
static void add_cjk_fallback(cui_ctx *ui) {
    cui_font prev = -1;
    for (int i = 0; i < DS_CJK_COUNT; i++) {
        cui_font f = cui_font_load_memory(ui, kCjkFonts[i].data, kCjkFonts[i].len, true);
        if (f < 0) continue;
        cui_font_add_fallback(ui, CUI_FONT_BODY, f);
        cui_font_add_fallback(ui, CUI_FONT_MEDIUM, f);
        cui_font_add_fallback(ui, CUI_FONT_MONO, f);
        if (prev >= 0) cui_font_add_fallback(ui, prev, f);
        prev = f;
    }
    ds_log_add(DS_LOG_OK, "CJK fallback fonts embedded (%d/%d/%d bytes)",
               (int)kCjkFonts[0].len, (int)kCjkFonts[1].len, (int)kCjkFonts[2].len);
}

int main(void) {
    ds_http_init();

    app_state a;
    app_state_init(&a);

    cui_app *app = cui_app_create(&(cui_app_desc){
        .title = "DeepSeek Audio",
        .width = 1280, .height = 840,
        .min_width = 940, .min_height = 620,
        .continuous = true,
        .follow_system_theme = false,   /* the app owns the theme; CUI would re-apply the OS theme every frame */
        .accent = CUI_ACCENT_BLUE,
        .app_id = "dev.dsaudio.dsaudio",
    });
    if (!app) return 1;


    double start = ds_now_sec();
    double next_theme_poll = 0.0;
    bool cjk_done = false;
    double quit_after = 0.0;
    const char *qa = getenv("DSAUDIO_QUIT_AFTER");
    if (qa && qa[0]) quit_after = atof(qa);

    while (cui_app_frame(app)) {
        cui_ctx *ui = cui_app_ui(app);
        if (!cjk_done) { cjk_done = true; add_cjk_fallback(ui); }
        double t = ds_now_sec();
        if (a.theme_mode == 0) cui_set_theme(ui, CUI_THEME_LIGHT);
        else if (a.theme_mode == 1) cui_set_theme(ui, CUI_THEME_DARK);
        else if (t - next_theme_poll > 1.0) {   /* "system": follow the OS at 1 Hz */
            next_theme_poll = t;
            cui_set_theme(ui, ds_system_dark() ? CUI_THEME_DARK : CUI_THEME_LIGHT);
        }

        app_state_poll_job(&a);

        if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_1)) a.page = PAGE_TTS;
        if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_2)) a.page = PAGE_ASR;
        if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_3)) a.page = PAGE_MCP;
        if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_4)) a.page = PAGE_SETTINGS;
        if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_G) && a.page == PAGE_TTS)
            app_state_request_tts(&a);
        if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_T) && a.page == PAGE_ASR)
            app_state_request_asr(&a);
        if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_B)) a.context_open = !a.context_open;

        cui_row(ui, .width = CUI_FILL, .height = CUI_FILL, .gap = 0, .bg = CUI_COLOR_BACKGROUND) {
            nav(ui, &a);
            cui_separator(ui);
            cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 24, .gap = 16,
                       .scroll_y = true) {
                switch (a.page) {
                case PAGE_TTS: ui_tts(ui, &a); break;
                case PAGE_ASR: ui_asr(ui, &a); break;
                case PAGE_MCP: ui_mcp(ui, &a); break;
                default: ui_settings(ui, &a); break;
                }
            }
        }

        cui_sheet(ui, "Session", &a.context_open, .side = CUI_SIDE_RIGHT, .size = 340) {
            cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 18, .gap = 14,
                       .scroll_y = true) {
                session_card(ui, &a);
                activity_card(ui, &a);
                cui_secondary(ui, "This window renders continuously so the waveform and progress "
                                  "stay live, which uses CPU while idle.");
            }
        }

        if (quit_after > 0.0 && ds_now_sec() - start >= quit_after) {
            cui_app_quit(app);
            break;
        }
    }

    app_state_free(&a);
    cui_app_destroy(app);
    return 0;
}
