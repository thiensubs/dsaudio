// ui_mcp.c - the MCP server: what it exposes, how to install it, and a live
// self test that runs the real binary.
#include "app_state.h"

#include "../core/ds_i18n.h"
#include "../core/ds_mcp.h"

#include <cui/cui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char mcp_path[1024];
static bool mcp_path_ok = false;
static char selftest[8192];
static bool selftest_ok = false;

static void find_mcp(void) {
    if (mcp_path_ok) return;
    char *self = ds_self_path();
    if (!self) return;
    char *slash = strrchr(self, '/');
#ifdef _WIN32
    char *bslash = strrchr(self, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    if (slash) {
        size_t dir_len = (size_t)(slash - self) + 1;
        snprintf(mcp_path, sizeof mcp_path, "%.*sdsaudio-mcp", (int)dir_len, self);
    } else {
        snprintf(mcp_path, sizeof mcp_path, "dsaudio-mcp");
    }
    free(self);
    mcp_path_ok = true;
}

static void run_selftest(cui_ctx *ui, app_state *a) {
    (void)ui;
    (void)a;
    ds_mcp_tools_list_json(selftest, sizeof selftest);
    selftest_ok = true;
}

void ui_mcp(cui_ctx *ui, app_state *a) {
    find_mcp();
    cui_row(ui, .gap = 8) {
        cui_heading(ui, ds_i18n("mcp.title"), 2);
        cui_spacer(ui);
        cui_badge(ui, selftest_ok ? ds_i18n("mcp.selftest_run") : ds_i18n("mcp.not_run"), .tone = CUI_TONE_NEUTRAL);
    }
    cui_secondary(ui, ds_i18n("mcp.subtitle"));

    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, ds_i18n("mcp.tools"), 4);
        static const char *names[] = {"tts_synthesize", "asr_transcribe", "config_get", "health"};
        const char *descs[] = {
            ds_i18n("mcp.tool_tts"),
            ds_i18n("mcp.tool_asr"),
            ds_i18n("mcp.tool_config"),
            ds_i18n("mcp.tool_health"),
        };
        for (int i = 0; i < 4; i++) {
            cui_row(ui, .gap = 8) {
                cui_text(ui, names[i], .font = CUI_FONT_MONO, .width = CUI_PX(150), .nowrap = true);
                cui_text(ui, descs[i], .color = CUI_COLOR_TEXT_SECONDARY);
            }
        }
        cui_separator(ui);
        cui_secondary(ui, ds_i18n("mcp.transport"));
    }

    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, ds_i18n("mcp.install"), 4);
        char snippet[1200];
        snprintf(snippet, sizeof snippet,
                 "%s\n"
                 "\"%s\" --install\n\n"
                 "# or, once a release is published:\n"
                 "curl -fsSL https://github.com/thiensubs/dsaudio/releases/latest/download/install.sh | sh",
                 ds_i18n("mcp.install_comment"), mcp_path);
        cui_code_block(ui, "mcp-install", snippet, .lang = "shell", .title = ds_i18n("mcp.install"));
        cui_secondary(ui, ds_i18n("mcp.install_desc"));

        char client[1200];
        snprintf(client, sizeof client,
                 "{\n  \"mcpServers\": {\n    \"deepseek-audio\": {\n"
                 "      \"command\": \"%s\",\n      \"args\": []\n    }\n  }\n}",
                 mcp_path);
        cui_code_block(ui, "mcp-client", client, .lang = "json", .title = ds_i18n("mcp.client_config"));
    }

    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_row(ui, .gap = 8) {
            cui_heading(ui, ds_i18n("mcp.self_test"), 4);
            cui_spacer(ui);
            if (cui_button(ui, selftest_ok ? ds_i18n("mcp.run_again") : ds_i18n("mcp.run"),
                           .icon = CUI_ICON_TERMINAL))
                run_selftest(ui, a);
        }
        if (!selftest_ok) {
            cui_secondary(ui, ds_i18n("mcp.self_test_desc"));
        } else {
            cui_json_view(ui, "mcp-selftest", selftest, .expand_depth = 2);
        }
    }
}
