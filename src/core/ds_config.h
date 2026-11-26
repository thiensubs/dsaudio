// ds_config.h - where the DeepSeek key and base URL come from.
//
// Precedence, highest first:
//   1. $DEEPSEEK_API_KEY / $DEEPSEEK_BASE_URL
//   2. ~/.config/dsaudio/config.json  (this app's own file, written by Settings)
//   3. ~/.kimi-code/config.toml [providers.DeepSeek]  (reuses the existing key)
//   4. https://api.deepseek.com
#ifndef DS_CONFIG_H
#define DS_CONFIG_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_config {
    char api_key[512];
    char base_url[256];
    char output_dir[512];
    char config_path[512];   /* file that is in force, or "" */
    char source[160];        /* human-readable note for the UI */
    char key_store[64];      /* "keychain", "protected file", "config file", ... */
    bool have_key;
    /* Provenance of base_url/output_dir: a config file may replace the built-in
     * default, but $DEEPSEEK_BASE_URL outranks every file. */
    bool base_from_env;
    bool base_from_file;
    bool out_from_file;

    /* Every tunable the app uses, editable in Settings and persisted here. */
    int tts_timeout_s;       /* 30..900 */
    int asr_timeout_s;       /* 60..1800 */
    int history_limit;       /* 8..512 */
    int peaks;               /* 32..1024 */
    char default_voice[16];  /* "stella" | "tide" */
    char default_format[8];  /* "wav" | "pcm" */
    bool filter_markdown;
    int theme;               /* 0 light, 1 dark, 2 system */
    char language[16];       /* "en", "vi", ... */
} ds_config;

void ds_config_init(ds_config *c);
bool ds_config_load(ds_config *c);              /* true when a key was found */
/* Writes the JSON always. `key_stored` (may be NULL) reports whether the key
 * reached the platform credential store, so a denied keychain prompt does not
 * throw away the rest of the settings. */
bool ds_config_save_ex(const ds_config *c, bool *key_stored, char *err, size_t cap);
bool ds_config_save(const ds_config *c, char *err, size_t cap);
bool ds_config_ensure_output_dir(const ds_config *c, char *err, size_t cap);
void ds_config_mask_key(const ds_config *c, char *out, size_t cap);

/* First existing of the candidate config files, or NULL. */
const char *ds_config_default_path(void);

#ifdef __cplusplus
}
#endif

#endif
