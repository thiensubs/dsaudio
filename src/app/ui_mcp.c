// ui_mcp.c - the MCP server: what it exposes, how to install it, and a live
// self test that runs the real binary.
#include "app_state.h"
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
        cui_heading(ui, "MCP server", 2);
        cui_spacer(ui);
        cui_badge(ui, selftest_ok ? "selftest run" : "not run yet", .tone = CUI_TONE_NEUTRAL);
    }
    cui_secondary(ui, "The same core, exposed over the Model Context Protocol on stdio. "
                      "An agent calls tts_synthesize or asr_transcribe and gets the result back.");

    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, "Tools", 4);
        static const char *names[] = {"tts_synthesize", "asr_transcribe", "config_get", "health"};
        static const char *descs[] = {
            "Text to audio. Writes the file, returns the audio and its metadata.",
            "Audio to text. WAV or Ogg Opus, language detected.",
            "Base URL, whether a key is set, output folder.",
            "Liveness and version.",
        };
        for (int i = 0; i < 4; i++) {
            cui_row(ui, .gap = 8) {
                cui_text(ui, names[i], .font = CUI_FONT_MONO, .width = CUI_PX(150), .nowrap = true);
                cui_text(ui, descs[i], .color = CUI_COLOR_TEXT_SECONDARY);
            }
        }
        cui_separator(ui);
        cui_secondary(ui, "stdio transport, JSON-RPC 2.0. One request per line, one response per line.");
    }

    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, "Install on this machine", 4);
        char snippet[1200];
        snprintf(snippet, sizeof snippet,
                 "# copy the server next to this app into ~/.local/bin\n"
                 "\"%s\" --install\n\n"
                 "# or, once a release is published:\n"
                 "curl -fsSL https://github.com/thiensubs/dsaudio/releases/latest/download/install.sh | sh",
                 mcp_path);
        cui_code_block(ui, "mcp-install", snippet, .lang = "shell", .title = "install");
        cui_secondary(ui, "The binary is self-contained: C, simdjson and libcurl only. "
                          "No Python, no Node, no runtime to install.");

        char client[1200];
        snprintf(client, sizeof client,
                 "{\n  \"mcpServers\": {\n    \"deepseek-audio\": {\n"
                 "      \"command\": \"%s\",\n      \"args\": []\n    }\n  }\n}",
                 mcp_path);
        cui_code_block(ui, "mcp-client", client, .lang = "json", .title = "client config");
    }

    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_row(ui, .gap = 8) {
            cui_heading(ui, "Self test", 4);
            cui_spacer(ui);
            if (cui_button(ui, selftest_ok ? "Run again" : "Run self test",
                           .icon = CUI_ICON_TERMINAL))
                run_selftest(ui, a);
        }
        if (!selftest_ok) {
            cui_secondary(ui, "Runs the server binary with --selftest and shows its tools/list reply.");
        } else {
            cui_json_view(ui, "mcp-selftest", selftest, .expand_depth = 2);
        }
    }
}
