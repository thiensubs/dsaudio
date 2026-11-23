// dsaudio-mcp - DeepSeek TTS/ASR over the Model Context Protocol (stdio transport).
//
//   dsaudio-mcp                  read JSON-RPC lines from stdin, write answers to stdout
//   dsaudio-mcp --selftest       print the tools/list result and exit
//   dsaudio-mcp --install [DIR]  copy this binary into DIR (default: the user's bin)
//   dsaudio-mcp --print-config   print the MCP client snippet for this machine
#include "../core/ds_account.h"
#include "../core/ds_asr.h"
#include "../core/ds_config.h"
#include "../core/ds_http.h"
#include "../core/ds_json.h"
#include "../core/ds_log.h"
#include "../core/ds_mcp.h"
#include "../core/ds_tts.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#define DS_MCP_VERSION "1.0.0"

/* --- small helpers ------------------------------------------------------- */

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void emit(const char *text) {
    fputs(text, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

static void id_token(const char *id_raw, char *out, size_t cap) {
    if (id_raw && id_raw[0]) snprintf(out, cap, "%s", id_raw);
    else snprintf(out, cap, "null");
}

static void emit_result(const char *id_raw, const char *result_json) {
    char id[256];
    id_token(id_raw, id, sizeof id);
    printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}\n", id, result_json);
    fflush(stdout);
}

static void emit_error(const char *id_raw, int code, const char *message) {
    ds_jsonw *e = ds_jsonw_new();
    ds_jsonw_set_i64(e, "code", code);
    ds_jsonw_set_str(e, "message", message);
    char *err = ds_jsonw_finish(e);
    char *inner = strchr(err, '{');
    memmove(err, inner, strlen(inner) + 1);

    char id[256];
    id_token(id_raw, id, sizeof id);
    printf("{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":%s}\n", id, err);
    fflush(stdout);
    free(err);
}

static void emit_text_result(const char *id_raw, const char *text, bool is_error) {
    ds_jsonw *item = ds_jsonw_new();
    ds_jsonw_set_str(item, "type", "text");
    ds_jsonw_set_str(item, "text", text);
    char *s = ds_jsonw_finish(item);
    char *body = (char *)malloc(strlen(s) + 64);
    sprintf(body, "{\"content\":[%s],\"isError\":%s}", s, is_error ? "true" : "false");
    free(s);
    emit_result(id_raw, body);
    free(body);
}

/* --- base64 -------------------------------------------------------------- */

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64_encode(const unsigned char *data, size_t len) {
    size_t out_len = ((len + 2) / 3) * 4;
    char *out = (char *)malloc(out_len + 1);
    if (!out) return NULL;
    size_t i, j = 0;
    for (i = 0; i + 2 < len; i += 3) {
        unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i + 1] << 8) | data[i + 2];
        out[j++] = kB64[(v >> 18) & 63];
        out[j++] = kB64[(v >> 12) & 63];
        out[j++] = kB64[(v >> 6) & 63];
        out[j++] = kB64[v & 63];
    }
    if (i < len) {
        unsigned v = (unsigned)data[i] << 16;
        bool two = (i + 1 < len);
        if (two) v |= (unsigned)data[i + 1] << 8;
        out[j++] = kB64[(v >> 18) & 63];
        out[j++] = kB64[(v >> 12) & 63];
        out[j++] = two ? kB64[(v >> 6) & 63] : '=';
        out[j++] = '=';
    }
    out[j] = '\0';
    return out;
}

/* --- tools --------------------------------------------------------------- */

static char *self_path(void) {
    char buf[1024];
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, buf, sizeof buf);
    if (n == 0 || n >= sizeof buf) return NULL;
#elif defined(__APPLE__)
    uint32_t n = sizeof buf;
    if (_NSGetExecutablePath(buf, &n) != 0) return NULL;
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n < 0) return NULL;
    buf[n] = '\0';
#endif
    return xstrdup(buf);
}

static void tools_list_result(char *out, size_t cap) {
    ds_mcp_tools_list_json(out, cap);
}

static void tool_tts(const ds_config *cfg, ds_json *args, const char *id_raw) {
    char text[16384] = "", voice[32] = "stella", format[32] = "wav";
    char out_path[600] = "";
    bool filter = true;
    ds_json_at_str(args, "params.arguments.text", text, sizeof text);
    ds_json_at_str(args, "params.arguments.voice", voice, sizeof voice);
    ds_json_at_str(args, "params.arguments.format", format, sizeof format);
    ds_json_at_str(args, "params.arguments.out_path", out_path, sizeof out_path);
    ds_json_at_bool(args, "params.arguments.filter_markdown", &filter);
    if (!text[0]) {
        emit_text_result(id_raw, "tts_synthesize: 'text' is required", true);
        return;
    }

    ds_tts_options opt = { text, voice, format, filter, 300 };
    ds_tts_result r;
    if (!ds_tts_run(cfg, &opt, &r)) {
        char msg[700];
        snprintf(msg, sizeof msg, "tts_synthesize failed: %s", r.err.text);
        emit_text_result(id_raw, msg, true);
        ds_tts_result_free(&r);
        return;
    }

    char err[300];
    if (!out_path[0]) {
        if (!ds_config_ensure_output_dir(cfg, err, sizeof err)) {
            emit_text_result(id_raw, err, true);
            ds_tts_result_free(&r);
            return;
        }
        snprintf(out_path, sizeof out_path, "%s/tts-%ld.%s",
                 cfg->output_dir, (long)time(NULL), format);
    }
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        char msg[700];
        snprintf(msg, sizeof msg, "tts_synthesize: cannot write %s", out_path);
        emit_text_result(id_raw, msg, true);
        ds_tts_result_free(&r);
        return;
    }
    fwrite(r.audio.data, 1, r.audio.len, f);
    fclose(f);

    char *b64 = base64_encode(r.audio.data, r.audio.len);
    const char *mime = (strcmp(format, "pcm") == 0) ? "audio/pcm" : "audio/wav";

    ds_jsonw *audio_item = ds_jsonw_new();
    ds_jsonw_set_str(audio_item, "type", "audio");
    ds_jsonw_set_str(audio_item, "data", b64 ? b64 : "");
    ds_jsonw_set_str(audio_item, "mimeType", mime);
    char *a = ds_jsonw_finish(audio_item);

    char summary[1200];
    snprintf(summary, sizeof summary,
             "{\"path\":\"%s\",\"bytes\":%zu,\"contentType\":\"%s\",\"voice\":\"%s\",\"format\":\"%s\"}",
             out_path, r.audio.len, r.content_type, voice, format);

    ds_jsonw *text_item = ds_jsonw_new();
    ds_jsonw_set_str(text_item, "type", "text");
    ds_jsonw_set_str(text_item, "text", summary);
    char *t = ds_jsonw_finish(text_item);

    char *body = (char *)malloc(strlen(a) + strlen(t) + 64);
    sprintf(body, "{\"content\":[%s,%s],\"isError\":false}", a, t);
    emit_result(id_raw, body);
    free(body);
    free(a);
    free(t);
    free(b64);
    ds_tts_result_free(&r);
}

static void tool_asr(const ds_config *cfg, ds_json *args, const char *id_raw) {
    char path[600] = "", out_path[600] = "";
    ds_json_at_str(args, "params.arguments.file_path", path, sizeof path);
    ds_json_at_str(args, "params.arguments.out_path", out_path, sizeof out_path);
    if (!path[0]) {
        emit_text_result(id_raw, "asr_transcribe: 'file_path' is required", true);
        return;
    }
    ds_asr_result r;
    if (!ds_asr_run(cfg, path, 600, &r)) {
        char msg[700];
        snprintf(msg, sizeof msg, "asr_transcribe failed: %s", r.err.text);
        emit_text_result(id_raw, msg, true);
        ds_asr_result_free(&r);
        return;
    }
    if (out_path[0]) {
        FILE *f = fopen(out_path, "wb");
        if (f) {
            fwrite(r.text, 1, strlen(r.text), f);
            fclose(f);
        }
    }
    char msg[4096];
    snprintf(msg, sizeof msg,
             "{\"duration_ms\":%lld,\"request_id\":\"%s\",\"out_path\":\"%s\"}\n\n%s",
             (long long)r.duration_ms, r.request_id, out_path, r.text ? r.text : "");
    emit_text_result(id_raw, msg, false);
    ds_asr_result_free(&r);
}

static void tool_config(const ds_config *cfg, const char *id_raw) {
    char masked[128];
    ds_config_mask_key(cfg, masked, sizeof masked);
    char out[1024];
    snprintf(out, sizeof out,
             "{\"base_url\":\"%s\",\"has_key\":%s,\"output_dir\":\"%s\",\"key\":\"%s\",\"source\":\"%s\"}",
             cfg->base_url, cfg->have_key ? "true" : "false", cfg->output_dir, masked,
             cfg->source);
    emit_text_result(id_raw, out, false);
}

static void tool_health(const char *id_raw) {
    char out[128];
    snprintf(out, sizeof out, "{\"status\":\"ok\",\"version\":\"%s\"}", DS_MCP_VERSION);
    emit_text_result(id_raw, out, false);
}

/* --- request handling ---------------------------------------------------- */

static void handle_request(ds_json *req) {
    char id_raw[256] = "";
    ds_json_raw(req, "id", id_raw, sizeof id_raw);
    char method[128] = "";
    ds_json_str(req, "method", method, sizeof method);

    if (strcmp(method, "notifications/initialized") == 0 || strncmp(method, "notifications/", 14) == 0)
        return;

    ds_config cfg;
    ds_config_load(&cfg);

    if (strcmp(method, "initialize") == 0) {
        char result[1024];
        snprintf(result, sizeof result,
                 "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},"
                 "\"serverInfo\":{\"name\":\"dsaudio-mcp\",\"version\":\"%s\"}}",
                 DS_MCP_VERSION);
        emit_result(id_raw, result);
    } else if (strcmp(method, "tools/list") == 0) {
        char result[4096];
        tools_list_result(result, sizeof result);
        emit_result(id_raw, result);
    } else if (strcmp(method, "ping") == 0) {
        emit_result(id_raw, "{}");
    } else if (strcmp(method, "tools/call") == 0) {
        char name[128] = "";
        ds_json_at_str(req, "params.name", name, sizeof name);
        if (strcmp(name, "tts_synthesize") == 0) tool_tts(&cfg, req, id_raw);
        else if (strcmp(name, "asr_transcribe") == 0) tool_asr(&cfg, req, id_raw);
        else if (strcmp(name, "config_get") == 0) tool_config(&cfg, id_raw);
        else if (strcmp(name, "health") == 0) tool_health(id_raw);
        else emit_error(id_raw, -32602, "unknown tool");
    } else if (method[0]) {
        emit_error(id_raw, -32601, "method not found");
    }
}

/* --- install ------------------------------------------------------------- */

static bool copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[65536];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    fclose(in);
    fclose(out);
    return ok;
}

static bool ensure_dir(const char *path) {
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

static int cmd_install(const char *prefix) {
    char *self = self_path();
    if (!self) {
        fprintf(stderr, "cannot resolve the running binary\n");
        return 1;
    }
    char dir[600], dest[800];
    if (prefix) {
        snprintf(dir, sizeof dir, "%s", prefix);
    } else {
#ifdef _WIN32
        const char *local = getenv("LOCALAPPDATA");
        snprintf(dir, sizeof dir, "%s\\dsaudio\\bin", local ? local : ".");
#else
        const char *home = getenv("HOME");
        snprintf(dir, sizeof dir, "%s/.local/bin", home ? home : ".");
#endif
    }
    if (!ensure_dir(dir)) {
        fprintf(stderr, "cannot create %s\n", dir);
        free(self);
        return 1;
    }
    snprintf(dest, sizeof dest, "%s/dsaudio-mcp", dir);
    if (!copy_file(self, dest)) {
        fprintf(stderr, "cannot write %s\n", dest);
        free(self);
        return 1;
    }
#ifndef _WIN32
    chmod(dest, 0755);
#endif
    printf("installed %s\n", dest);
    printf("add this to your MCP client:\n\n");
    char cmd[900];
    snprintf(cmd, sizeof cmd,
             "{\n  \"mcpServers\": {\n    \"deepseek-audio\": {\n"
             "      \"command\": \"%s\",\n      \"args\": []\n    }\n  }\n}\n",
             dest);
    fputs(cmd, stdout);
    free(self);
    return 0;
}

int main(int argc, char **argv) {
    ds_http_init();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--selftest") == 0) {
            char result[4096];
            tools_list_result(result, sizeof result);
            emit(result);
            return 0;
        }
        if (strcmp(argv[i], "--print-config") == 0) {
            char *self = self_path();
            printf("{\n  \"mcpServers\": {\n    \"deepseek-audio\": {\n"
                   "      \"command\": \"%s\",\n      \"args\": []\n    }\n  }\n}\n",
                   self ? self : "dsaudio-mcp");
            free(self);
            return 0;
        }
        if (strcmp(argv[i], "--install") == 0)
            return cmd_install(i + 1 < argc ? argv[++i] : NULL);
        if (strcmp(argv[i], "--version") == 0) {
            printf("dsaudio-mcp %s\n", DS_MCP_VERSION);
            return 0;
        }
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, stdin)) > 0) {
        ds_json *req = ds_json_parse(line, (size_t)n);
        if (!req) {
            emit_error("null", -32700, "parse error");
            continue;
        }
        handle_request(req);
        ds_json_free(req);
    }
    free(line);
    return 0;
}
