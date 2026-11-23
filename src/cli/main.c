// dsaudio-cli - the dsaudio core without a window.
//
//   dsaudio-cli tts "text" [--voice stella|tide] [--format wav|pcm] [--out FILE]
//   dsaudio-cli asr FILE [--out FILE]
//   dsaudio-cli config
#include "../core/ds_asr.h"
#include "../core/ds_audio.h"
#include "../core/ds_config.h"
#include "../core/ds_http.h"
#include "../core/ds_keychain.h"
#include "../core/ds_log.h"
#include "../core/ds_tts.h"
#include "../core/ds_tts_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void stamp(char *out, size_t cap) {
    time_t t = time(NULL);
    struct tm tm;
    ds_localtime(t, &tm);
    strftime(out, cap, "%Y%m%d-%H%M%S", &tm);
}

static int cmd_config(void) {
    ds_config cfg;
    bool have = ds_config_load(&cfg);
    char masked[128];
    ds_config_mask_key(&cfg, masked, sizeof masked);
    printf("base_url:    %s\n", cfg.base_url);
    printf("api_key:     %s\n", masked);
    printf("output_dir:  %s\n", cfg.output_dir);
    printf("key_store:   %s\n", cfg.key_store[0] ? cfg.key_store : "-");
    printf("source:      %s\n", cfg.source);
    printf("config_file: %s\n", ds_config_default_path());
    return have ? 0 : 2;
}

static int cmd_setkey(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: dsaudio-cli setkey <api-key>\n");
        return 2;
    }
    ds_config cfg;
    ds_config_load(&cfg);
    snprintf(cfg.api_key, sizeof cfg.api_key, "%s", argv[2]);
    char err[300];
    if (!ds_config_save(&cfg, err, sizeof err)) {
        fprintf(stderr, "setkey: %s\n", err);
        return 1;
    }
    char masked[128];
    ds_config_mask_key(&cfg, masked, sizeof masked);
    printf("stored %s in the %s\n", masked, ds_keychain_backend_name());
    printf("config: %s\n", err);
    return 0;
}

static int cmd_tts(int argc, char **argv) {
    const char *text = NULL, *voice = "stella", *format = "wav", *out = NULL;
    bool filter = true, chunks = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--voice") == 0 && i + 1 < argc) voice = argv[++i];
        else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) format = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
        else if (strcmp(argv[i], "--no-filter-markdown") == 0) filter = false;
        else if (strcmp(argv[i], "--chunks") == 0) chunks = true;
        else if (!text && argv[i][0] != '-') text = argv[i];
        else if (strcmp(argv[i], "-") == 0 && !text) {
            static char buf[65536];
            size_t n = fread(buf, 1, sizeof buf - 1, stdin);
            buf[n] = '\0';
            text = buf;
        }
    }
    if (!text) {
        fprintf(stderr, "usage: dsaudio-cli tts \"text\" [- for stdin] [--voice stella|tide] "
                        "[--format wav|pcm] [--out FILE] [--no-filter-markdown]\n");
        return 2;
    }

    ds_config cfg;
    ds_config_load(&cfg);

    if (chunks) {
        ds_tts_chunk_result r;
        if (!ds_tts_synth_cached(&cfg, text, voice, format, filter, &r, NULL, NULL)) {
            fprintf(stderr, "tts: %s\n", r.err);
            ds_tts_chunk_result_free(&r);
            return 1;
        }
        const char *o = out ? out : "/tmp/dsaudio-chunks.wav";
        FILE *f = fopen(o, "wb");
        if (!f) { fprintf(stderr, "tts: cannot write %s\n", o); ds_tts_chunk_result_free(&r); return 1; }
        fwrite(r.audio.data, 1, r.audio.len, f);
        fclose(f);
        printf("%s (%zu bytes, %s) - %d chunks, %d cached, %d synthesized\n",
               o, r.audio.len, r.content_type, r.chunks, r.cached, r.generated);
        ds_tts_chunk_result_free(&r);
        return 0;
    }

    ds_tts_options opt = {text, voice, format, filter, 300};
    ds_tts_result r;
    if (!ds_tts_run(&cfg, &opt, &r)) {
        fprintf(stderr, "tts: %s\n", r.err.text);
        ds_tts_result_free(&r);
        return 1;
    }

    if (!out) {
        char s[32];
        stamp(s, sizeof s);
        char path[600];
        snprintf(path, sizeof path, "%s/tts-%s.%s", cfg.output_dir, s, format);
        out = path;
    }
    /* the CLI has no mkdir helper of its own; the core writes only the file */
    FILE *f = fopen(out, "wb");
    if (!f) {
        fprintf(stderr, "tts: cannot write %s\n", out);
        ds_tts_result_free(&r);
        return 1;
    }
    fwrite(r.audio.data, 1, r.audio.len, f);
    fclose(f);
    printf("%s (%zu bytes, %s)\n", out, r.audio.len, r.content_type);
    ds_tts_result_free(&r);
    return 0;
}

static int cmd_asr(int argc, char **argv) {
    const char *file = NULL, *out = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
        else if (!file) file = argv[i];
    }
    if (!file) {
        fprintf(stderr, "usage: dsaudio-cli asr FILE [--out FILE]\n");
        return 2;
    }

    ds_config cfg;
    ds_config_load(&cfg);
    ds_asr_result r;
    if (!ds_asr_run(&cfg, file, 600, &r)) {
        fprintf(stderr, "asr: %s\n", r.err.text);
        ds_asr_result_free(&r);
        return 1;
    }
    if (out) {
        FILE *f = fopen(out, "wb");
        if (!f) {
            fprintf(stderr, "asr: cannot write %s\n", out);
            ds_asr_result_free(&r);
            return 1;
        }
        fwrite(r.text, 1, strlen(r.text), f);
        fclose(f);
        printf("%s (%zu chars)\n", out, strlen(r.text));
    } else {
        printf("%s\n", r.text);
    }
    if (r.duration_ms > 0)
        fprintf(stderr, "request_id=%s duration_ms=%lld\n", r.request_id,
                (long long)r.duration_ms);
    ds_asr_result_free(&r);
    return 0;
}

int main(int argc, char **argv) {
    ds_http_init();
    if (argc < 2) {
        fprintf(stderr, "usage: dsaudio-cli <tts|asr|config|setkey> ...\n");
        return 2;
    }
    if (strcmp(argv[1], "tts") == 0) return cmd_tts(argc, argv);
    if (strcmp(argv[1], "asr") == 0) return cmd_asr(argc, argv);
    if (strcmp(argv[1], "config") == 0) return cmd_config();
    if (strcmp(argv[1], "setkey") == 0) return cmd_setkey(argc, argv);
    fprintf(stderr, "unknown command '%s'\n", argv[1]);
    return 2;
}
