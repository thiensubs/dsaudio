#include "ds_config.h"

#include "ds_json.h"
#include "ds_tts.h"
#include "ds_keychain.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

static void home_dir(char *out, size_t cap) {
#ifdef _WIN32
    const char *local = getenv("LOCALAPPDATA");
    if (local && local[0]) {
        snprintf(out, cap, "%s", local);
        return;
    }
#endif
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(out, cap, "%s", home);
        return;
    }
    snprintf(out, cap, ".");
}

const char *ds_config_default_path(void) {
    static char path[600];
    char home[400];
    home_dir(home, sizeof home);
#ifdef _WIN32
    snprintf(path, sizeof path, "%s\\dsaudio\\config.json", home);
#else
    snprintf(path, sizeof path, "%s/.config/dsaudio/config.json", home);
#endif
    return path;
}

static void default_output_dir(char *out, size_t cap) {
    char home[400];
    home_dir(home, sizeof home);
#ifdef _WIN32
    snprintf(out, cap, "%s\\dsaudio\\output", home);
#else
    snprintf(out, cap, "%s/.dsaudio/output", home);
#endif
}

static bool read_whole(const char *path, char **out, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    ds_buf b;
    ds_buf_init(&b);
    char chunk[8192];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0)
        if (!ds_buf_append(&b, chunk, n)) {
            ds_buf_free(&b);
            fclose(f);
            return false;
        }
    fclose(f);
    if (!b.data) {
        b.data = (unsigned char *)malloc(1);
        if (!b.data) return false;
        b.data[0] = '\0';
    }
    *out = (char *)b.data;
    *out_len = b.len;
    return true;
}

/* Fills only fields that are still empty, so the caller controls precedence. */
static const char *kKeyService = "dev.dsaudio.dsaudio";
static int json_int(ds_json *j, const char *key, int lo, int hi, int dflt) {
    int64_t v;
    if (ds_json_i64(j, key, &v) && v >= lo && v <= hi) return (int)v;
    return dflt;
}

static bool json_bool(ds_json *j, const char *key, bool dflt) {
    bool v;
    return ds_json_bool(j, key, &v) ? v : dflt;
}
static const char *kKeyAccount = "deepseek-api-key";

/* The key is read from the platform credential store, then (for a config file
 * written by an older version) from the JSON, then from kimi-code. */
static void keychain_load(ds_config *c, char *note, size_t note_cap) {
    char key[512];
    char err[256];
    if (ds_keychain_get(kKeyService, kKeyAccount, key, sizeof key, err, sizeof err) && key[0]) {
        snprintf(c->api_key, sizeof c->api_key, "%s", key);
        snprintf(c->key_store, sizeof c->key_store, "%s", ds_keychain_backend_name());
        snprintf(note, note_cap, "key from the %s", ds_keychain_backend_name());
    }
}

static void parse_json_config(const char *path, ds_config *c, char *note, size_t note_cap) {
    char *raw = NULL;
    size_t len = 0;
    if (!read_whole(path, &raw, &len)) return;
    ds_json *j = ds_json_parse(raw, len);
    free(raw);
    if (!j) return;

    char val[512];
    if (!c->api_key[0] && ds_json_str(j, "api_key", val, sizeof val) && val[0]) {
        snprintf(c->api_key, sizeof c->api_key, "%s", val);
        snprintf(c->config_path, sizeof c->config_path, "%s", path);
        snprintf(note, note_cap, "key from %s (plain text - move it to the keychain)", path);
        snprintf(c->key_store, sizeof c->key_store, "config file");
    }
    if (!c->base_from_env && !c->base_from_file &&
        ds_json_str(j, "base_url", val, sizeof val) && val[0]) {
        snprintf(c->base_url, sizeof c->base_url, "%s", val);
        c->base_from_file = true;
    }
    if (!c->out_from_file && ds_json_str(j, "output_dir", val, sizeof val) && val[0]) {
        snprintf(c->output_dir, sizeof c->output_dir, "%s", val);
        c->out_from_file = true;
    }
    c->tts_timeout_s = json_int(j, "tts_timeout_s", 30, 900, c->tts_timeout_s);
    c->asr_timeout_s = json_int(j, "asr_timeout_s", 60, 1800, c->asr_timeout_s);
    c->history_limit = json_int(j, "history_limit", 8, 512, c->history_limit);
    c->peaks = json_int(j, "peaks", 32, 1024, c->peaks);
    c->filter_markdown = json_bool(j, "filter_markdown", c->filter_markdown);
    c->theme = json_int(j, "theme", 0, 2, c->theme);
    if (ds_json_str(j, "default_voice", val, sizeof val) &&
        (strcmp(val, "stella") == 0 || strcmp(val, "tide") == 0))
        snprintf(c->default_voice, sizeof c->default_voice, "%s", val);
    if (ds_json_str(j, "default_format", val, sizeof val) &&
        (strcmp(val, "wav") == 0 || strcmp(val, "pcm") == 0))
        snprintf(c->default_format, sizeof c->default_format, "%s", val);
    if (ds_json_str(j, "language", val, sizeof val) && strlen(val) >= 2 && strlen(val) <= 8)
        snprintf(c->language, sizeof c->language, "%s", val);
    ds_json_free(j);
}

/* Minimal TOML: the [providers.DeepSeek] block of kimi-code's config. Only the
 * three keys this app needs are read, and only from quoted basic strings.
 * Nothing is ever written back to that file. */
static void parse_toml_config(const char *path, ds_config *c, char *note, size_t note_cap) {
    char *raw = NULL;
    size_t len = 0;
    if (!read_whole(path, &raw, &len)) return;

    const char *p = raw;
    bool in_block = false;
    char api_key_env[128] = "";

    while (*p) {
        const char *eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);
        const char *line = p;
        size_t llen = (size_t)(eol - p);

        const char *s = line;
        while (s < line + llen && (*s == ' ' || *s == '\t')) s++;

        if (*s == '[') {
            char sec[128];
            size_t k = 0;
            const char *q = s + 1;
            while (q < line + llen && *q != ']' && k + 1 < sizeof sec) sec[k++] = *q++;
            sec[k] = '\0';
            in_block = (strcmp(sec, "providers.DeepSeek") == 0);
        } else if (in_block && *s && *s != '#') {
            char key[64] = "";
            const char *q = s;
            size_t k = 0;
            while (q < line + llen && *q != '=' && *q != ' ' && *q != '\t' && k + 1 < sizeof key)
                key[k++] = *q++;
            key[k] = '\0';
            while (q < line + llen && (*q == ' ' || *q == '\t' || *q == '=')) q++;
            if (q < line + llen && (*q == '"' || *q == '\'')) {
                char quote = *q++;
                char value[512];
                k = 0;
                while (q < line + llen && *q != quote && k + 1 < sizeof value) value[k++] = *q++;
                value[k] = '\0';
                if (strcmp(key, "api_key") == 0 && !c->api_key[0] && value[0]) {
                    snprintf(c->api_key, sizeof c->api_key, "%s", value);
                    snprintf(c->config_path, sizeof c->config_path, "%s", path);
                    snprintf(note, note_cap, "key from %s [providers.DeepSeek]", path);
                    snprintf(c->key_store, sizeof c->key_store, "kimi-code config");
                } else if (strcmp(key, "api_key_env") == 0 && !api_key_env[0]) {
                    snprintf(api_key_env, sizeof api_key_env, "%s", value);
                } else if (strcmp(key, "base_url") == 0 && !c->base_from_env &&
                           !c->base_from_file && value[0]) {
                    snprintf(c->base_url, sizeof c->base_url, "%s", value);
                    c->base_from_file = true;
                }
            }
        }
        p = (*eol == '\n') ? eol + 1 : eol;
    }
    free(raw);

    if (api_key_env[0] && !c->api_key[0]) {
        const char *v = getenv(api_key_env);
        if (v && v[0]) {
            snprintf(c->api_key, sizeof c->api_key, "%s", v);
            snprintf(note, note_cap, "key from $%s (kimi-code config)", api_key_env);
            snprintf(c->key_store, sizeof c->key_store, "environment");
        }
    }
}

void ds_config_init(ds_config *c) {
    memset(c, 0, sizeof *c);
    snprintf(c->base_url, sizeof c->base_url, "https://api.deepseek.com");
    default_output_dir(c->output_dir, sizeof c->output_dir);
    snprintf(c->source, sizeof c->source, "defaults");
    snprintf(c->key_store, sizeof c->key_store, "-");
    c->tts_timeout_s = 300;
    c->asr_timeout_s = 600;
    c->history_limit = 64;
    c->peaks = DS_TTS_PEAKS;
    snprintf(c->default_voice, sizeof c->default_voice, "stella");
    snprintf(c->default_format, sizeof c->default_format, "wav");
    c->filter_markdown = true;
    c->theme = 2;
    snprintf(c->language, sizeof c->language, "en");
}


bool ds_config_load(ds_config *c) {
    ds_config_init(c);

    char note[200] = "";
    char own_note[200] = "";
    char toml_note[200] = "";

    keychain_load(c, note, sizeof note);

    const char *env_key = getenv("DEEPSEEK_API_KEY");
    const char *env_base = getenv("DEEPSEEK_BASE_URL");
    if (env_key && env_key[0]) {
        snprintf(c->api_key, sizeof c->api_key, "%s", env_key);
        snprintf(note, sizeof note, "key from $DEEPSEEK_API_KEY");
        snprintf(c->key_store, sizeof c->key_store, "environment");
    }
    if (env_base && env_base[0]) {
        snprintf(c->base_url, sizeof c->base_url, "%s", env_base);
        c->base_from_env = true;
    }

    char home[400];
    home_dir(home, sizeof home);
    char own_path[600], kimi[600];
#ifdef _WIN32
    snprintf(own_path, sizeof own_path, "%s\\dsaudio\\config.json", home);
    snprintf(kimi, sizeof kimi, "%s\\.kimi-code\\config.toml", home);
#else
    snprintf(own_path, sizeof own_path, "%s/.config/dsaudio/config.json", home);
    snprintf(kimi, sizeof kimi, "%s/.kimi-code/config.toml", home);
#endif

    parse_json_config(own_path, c, own_note, sizeof own_note);
    parse_toml_config(kimi, c, toml_note, sizeof toml_note);

    if (!note[0]) snprintf(note, sizeof note, "%s", own_note);
    if (!note[0]) snprintf(note, sizeof note, "%s", toml_note);

    c->have_key = c->api_key[0] != '\0';
    if (!c->have_key)
        snprintf(note, sizeof note, "no key: set DEEPSEEK_API_KEY or add one in Settings");
    snprintf(c->source, sizeof c->source, "%s", note);
    return c->have_key;
}

static bool ensure_dir(const char *path) {
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    char parent[900];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        *slash = '\0';
        ensure_dir(parent);
    }
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

bool ds_config_save_ex(const ds_config *c, bool *key_stored, char *err, size_t cap) {
    const char *path = ds_config_default_path();
    char dir[600];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
#ifdef _WIN32
    char *bslash = strrchr(dir, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    if (slash) {
        *slash = '\0';
        if (!ensure_dir(dir)) {
            snprintf(err, cap, "cannot create %s", dir);
            return false;
        }
    }

    ds_jsonw *w = ds_jsonw_new();
    ds_jsonw_set_str(w, "key_store", ds_keychain_backend_name());
    ds_jsonw_set_str(w, "base_url", c->base_url);
    ds_jsonw_set_str(w, "output_dir", c->output_dir);
    ds_jsonw_set_i64(w, "tts_timeout_s", c->tts_timeout_s);
    ds_jsonw_set_i64(w, "asr_timeout_s", c->asr_timeout_s);
    ds_jsonw_set_i64(w, "history_limit", c->history_limit);
    ds_jsonw_set_i64(w, "peaks", c->peaks);
    ds_jsonw_set_str(w, "default_voice", c->default_voice);
    ds_jsonw_set_str(w, "default_format", c->default_format);
    ds_jsonw_set_bool(w, "filter_markdown", c->filter_markdown);
    ds_jsonw_set_i64(w, "theme", c->theme);
    ds_jsonw_set_str(w, "language", c->language);
    char *text = ds_jsonw_finish(w);

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(text);
        snprintf(err, cap, "cannot write %s", path);
        return false;
    }
    size_t n = fwrite(text, 1, strlen(text), f);
    fclose(f);
    free(text);
    if (n == 0) {
        snprintf(err, cap, "write failed: %s", path);
        return false;
    }

    bool key_ok = true;
    char kerr[256];
    if (c->api_key[0]) {
        if (!ds_keychain_set(kKeyService, kKeyAccount, c->api_key, kerr, sizeof kerr)) {
            snprintf(err, cap, "settings saved to %s, but the key was NOT stored: %s "
                               "(click Allow on the keychain prompt, then save again)",
                     path, kerr);
            key_ok = false;
        }
    } else {
        ds_keychain_delete(kKeyService, kKeyAccount, kerr, sizeof kerr);
    }
    if (key_stored) *key_stored = key_ok;
    return key_ok;
}

bool ds_config_save(const ds_config *c, char *err, size_t cap) {
    return ds_config_save_ex(c, NULL, err, cap);
}

bool ds_config_ensure_output_dir(const ds_config *c, char *err, size_t cap) {
    if (!c->output_dir[0]) {
        snprintf(err, cap, "no output directory configured");
        return false;
    }
    if (ensure_dir(c->output_dir)) return true;
    snprintf(err, cap, "cannot create %s", c->output_dir);
    return false;
}

void ds_config_mask_key(const ds_config *c, char *out, size_t cap) {
    if (!c->have_key) {
        snprintf(out, cap, "not set");
        return;
    }
    size_t n = strlen(c->api_key);
    if (n <= 8) {
        snprintf(out, cap, "********");
        return;
    }
    snprintf(out, cap, "%.4s...%.4s (%zu chars)", c->api_key, c->api_key + n - 4, n);
}
