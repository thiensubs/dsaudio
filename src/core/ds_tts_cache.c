#include "ds_tts_cache.h"

#include "ds_audio.h"
#include "ds_config.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#endif

#define DS_CHUNK_TARGET 900     /* billed characters aimed for per chunk */
#define DS_CHUNK_MAX 2200       /* never exceed this in one chunk */

/* --- cache directory -------------------------------------------------------- */

const char *ds_cache_dir(void) {
    static char path[600];
    if (path[0]) return path;
#ifdef _WIN32
    const char *local = getenv("LOCALAPPDATA");
    snprintf(path, sizeof path, "%s\\dsaudio\\cache", local ? local : ".");
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/Library/Caches/dsaudio", home ? home : ".");
#else
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CACHE_HOME");
    if (xdg && xdg[0]) snprintf(path, sizeof path, "%s/dsaudio", xdg);
    else snprintf(path, sizeof path, "%s/.cache/dsaudio", home ? home : ".");
#endif
    return path;
}

static bool mkdir_p(const char *path) {
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#endif
#if !defined(_WIN32)
    char tmp[900];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }
    return mkdir(tmp, 0700) == 0 || errno == EEXIST;
#endif
}

bool ds_cache_clear(char *err, size_t cap) {
    const char *dir = ds_cache_dir();
    if (!mkdir_p(dir)) {
        snprintf(err, cap, "cannot create %s", dir);
        return false;
    }
#ifdef _WIN32
    char pattern[1000];
    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return true;   /* nothing cached */
    int failed = 0;
    do {
        if (fd.cFileName[0] == '.') continue;
        char path[1200];
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        if (!DeleteFileA(path)) failed++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (failed) snprintf(err, cap, "%d cached file(s) could not be removed", failed);
    return failed == 0;
#else
    DIR *d = opendir(dir);
    if (!d) {
        snprintf(err, cap, "cannot open %s", dir);
        return false;
    }
    int failed = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (unlink(path) != 0) failed++;
    }
    closedir(d);
    if (failed) snprintf(err, cap, "%d cached file(s) could not be removed", failed);
    return failed == 0;
#endif
}

/* --- chunking ---------------------------------------------------------------- */

static bool is_sentence_end(unsigned char c) {
    return c == '.' || c == '!' || c == '?' || c == '\n';
}

int ds_tts_split(const char *text, int max_chars, int *starts, int *lens, int max_chunks) {
    int n = 0;
    if (!text || !starts || !lens || max_chunks <= 0) return 0;
    int len = (int)strlen(text);
    if (len == 0) return 0;

    int at = 0;
    while (at < len && n < max_chunks) {
        int stop = at + max_chars;
        if (stop >= len) stop = len;

        /* Prefer the last sentence end inside the window. */
        int cut = -1;
        for (int i = stop - 1; i > at + max_chars / 2; i--) {
            if (is_sentence_end((unsigned char)text[i])) { cut = i + 1; break; }
        }
        if (cut < 0) {
            /* No sentence end: cut at the last space, else at the window. */
            for (int i = stop - 1; i > at; i--) {
                if (text[i] == ' ') { cut = i + 1; break; }
            }
        }
        if (cut <= at) cut = stop;
        if (cut > len) cut = len;

        starts[n] = at;
        lens[n] = cut - at;
        n++;
        at = cut;
        /* Skip a run of whitespace between chunks. */
        while (at < len && (text[at] == ' ' || text[at] == '\n' || text[at] == '\t')) at++;
    }
    return n;
}

/* --- cache key --------------------------------------------------------------- */

static void fnv1a128(const char *s, uint64_t out[2]) {
    uint64_t h1 = 0xcbf29ce484222325ULL, h2 = 0x9e3779b97f4a7c15ULL;
    for (; *s; s++) {
        h1 = (h1 ^ (unsigned char)*s) * 0x100000001b3ULL;
        h2 = (h2 + (unsigned char)*s) * 0x9e3779b97f4a7c15ULL;
    }
    out[0] = h1;
    out[1] = h2;
}

/* --- crossfade --------------------------------------------------------------- */

static void append_crossfade(ds_buf *out, const unsigned char *pcm, size_t bytes,
                             int sample_rate, int channels) {
    if (bytes == 0) return;
    size_t frames = bytes / (size_t)(channels * 2);
    if (out->len == 0 || frames == 0) {
        ds_buf_append(out, pcm, bytes);
        return;
    }
    size_t fade = (size_t)(sample_rate / 1000) * 12 * (size_t)channels;  /* 12 ms */
    if (fade > frames) fade = frames;
    size_t old_frames = out->len / (size_t)(channels * 2);
    size_t keep = old_frames > fade ? old_frames - fade : 0;
    out->len = keep * (size_t)(channels * 2);
    out->data[out->len] = '\0';

    short *a = (short *)(out->data + out->len);
    const short *b = (const short *)pcm;
    for (size_t f = 0; f < fade; f++) {
        double t = (double)f / (double)fade;
        double g1 = cos(t * 1.5707963267948966);   /* equal-power crossfade */
        double g2 = sin(t * 1.5707963267948966);
        for (int c = 0; c < channels; c++) {
            double v = (double)a[(size_t)f * channels + c] * g1 + (double)b[(size_t)f * channels + c] * g2;
            if (v > 32767.0) v = 32767.0;
            if (v < -32768.0) v = -32768.0;
            a[(size_t)f * channels + c] = (short)v;
        }
    }
    size_t rest = bytes - fade * (size_t)(channels * 2);
    ds_buf_append(out, pcm + fade * (size_t)(channels * 2), rest);
}

/* --- synthesis --------------------------------------------------------------- */

void ds_tts_chunk_result_free(ds_tts_chunk_result *r) {
    if (!r) return;
    ds_buf_free(&r->audio);
    memset(r, 0, sizeof *r);
}

static bool wrap_wav(ds_buf *out, const unsigned char *pcm, size_t bytes,
                     int sample_rate, int channels) {
    uint32_t data_bytes = (uint32_t)bytes;
    ds_buf_append(out, "RIFF", 4);
    uint32_t riff = 36u + data_bytes;
    unsigned char p[4];
    p[0] = (unsigned char)riff; p[1] = (unsigned char)(riff >> 8);
    p[2] = (unsigned char)(riff >> 16); p[3] = (unsigned char)(riff >> 24);
    ds_buf_append(out, p, 4);
    ds_buf_append(out, "WAVEfmt ", 8);
    uint32_t one = 16;
    p[0] = 16; p[1] = 0; p[2] = 0; p[3] = 0;
    ds_buf_append(out, p, 4);
    uint16_t fmt = 1, ch = (uint16_t)channels;
    p[0] = 1; p[1] = 0; ds_buf_append(out, p, 2);
    p[0] = (unsigned char)ch; p[1] = (unsigned char)(ch >> 8); ds_buf_append(out, p, 2);
    p[0] = (unsigned char)sample_rate; p[1] = (unsigned char)(sample_rate >> 8);
    p[2] = (unsigned char)(sample_rate >> 16); p[3] = (unsigned char)(sample_rate >> 24);
    ds_buf_append(out, p, 4);
    uint32_t byte_rate = sample_rate * channels * 2u;
    p[0] = (unsigned char)byte_rate; p[1] = (unsigned char)(byte_rate >> 8);
    p[2] = (unsigned char)(byte_rate >> 16); p[3] = (unsigned char)(byte_rate >> 24);
    ds_buf_append(out, p, 4);
    uint16_t align = (uint16_t)(channels * 2);
    p[0] = (unsigned char)align; p[1] = (unsigned char)(align >> 8); ds_buf_append(out, p, 2);
    p[0] = 16; p[1] = 0; ds_buf_append(out, p, 2);
    ds_buf_append(out, "data", 4);
    p[0] = (unsigned char)data_bytes; p[1] = (unsigned char)(data_bytes >> 8);
    p[2] = (unsigned char)(data_bytes >> 16); p[3] = (unsigned char)(data_bytes >> 24);
    ds_buf_append(out, p, 4);
    ds_buf_append(out, pcm, bytes);
    return true;
}

bool ds_tts_synth_cached(const ds_config *cfg, const char *text, const char *voice,
                         const char *format, bool filter_markdown,
                         ds_tts_chunk_result *out,
                         ds_tts_progress progress, void *user) {
    memset(out, 0, sizeof *out);
    out->ok = false;

    if (!text || !text[0]) {
        ds_err_set(&out->err, "no text to read");
        return false;
    }

    int starts[256], lens[256];
    int n = ds_tts_split(text, DS_CHUNK_TARGET, starts, lens, 256);
    if (n <= 0) {
        ds_err_set(&out->err, "the text could not be split");
        return false;
    }
    out->chunks = n;

    const char *dir = ds_cache_dir();
    if (!mkdir_p(dir)) {
        ds_err_set(&out->err, "cannot create the chunk cache at %s", dir);
        return false;
    }

    int sample_rate = 24000, channels = 1;
    ds_buf out_buf;
    ds_buf_init(&out_buf);

    for (int i = 0; i < n; i++) {
        char chunk[DS_CHUNK_MAX + 1];
        int cl = lens[i];
        if (cl > DS_CHUNK_MAX) cl = DS_CHUNK_MAX;
        memcpy(chunk, text + starts[i], (size_t)cl);
        chunk[cl] = '\0';

        uint64_t h[2];
        fnv1a128(chunk, h);
        char key[80];
        snprintf(key, sizeof key, "%016llx%016llx.%s", (unsigned long long)h[0],
                 (unsigned long long)h[1], format);

        char path[900];
        snprintf(path, sizeof path, "%s/%s", dir, key);

        ds_buf pcm;
        ds_buf_init(&pcm);
        bool have = false;

        FILE *f = fopen(path, "rb");
        if (f) {
            char buf[65536];
            size_t got;
            while ((got = fread(buf, 1, sizeof buf, f)) > 0)
                if (!ds_buf_append(&pcm, buf, got)) break;
            fclose(f);
            if (pcm.len > 0) {
                have = true;
                out->cached++;
            }
        }

        if (!have) {
            ds_tts_options opt = { chunk, voice, format, filter_markdown, cfg->tts_timeout_s };
            ds_tts_result r;
            if (!ds_tts_run(cfg, &opt, &r)) {
                ds_buf_free(&pcm);
                ds_buf_free(&out_buf);
                ds_err_set(&out->err, "chunk %d of %d failed: %s", i + 1, n, r.err.text);
                ds_tts_result_free(&r);
                return false;
            }
            /* normalise to 16-bit PCM so the join is sample-exact */
            ds_audio_info info;
            if (ds_audio_probe(r.audio.data, r.audio.len, &info)) {
                sample_rate = info.sample_rate ? info.sample_rate : 24000;
                channels = info.channels ? info.channels : 1;
            }
            if (strcmp(format, "pcm") == 0) {
                ds_buf_append(&pcm, r.audio.data, r.audio.len);
            } else {
                /* strip the WAV header: ASR's WAV is PCM16, so copy the data chunk */
                if (info.data_bytes && (uint64_t)info.data_offset + info.data_bytes <= r.audio.len)
                    ds_buf_append(&pcm, r.audio.data + info.data_offset, (size_t)info.data_bytes);
                else
                    ds_buf_append(&pcm, r.audio.data, r.audio.len);
            }
            FILE *w = fopen(path, "wb");
            if (w) {
                fwrite(pcm.data, 1, pcm.len, w);
                fclose(w);
            }
            out->generated++;
            ds_tts_result_free(&r);
        }

        append_crossfade(&out_buf, pcm.data, pcm.len, sample_rate, channels);
        ds_buf_free(&pcm);
        if (progress) progress(user, i + 1, n);
    }

    if (strcmp(format, "wav") == 0) {
        ds_buf wav;
        ds_buf_init(&wav);
        wrap_wav(&wav, out_buf.data, out_buf.len, sample_rate, channels);
        ds_buf_free(&out_buf);
        out->audio = wav;
        snprintf(out->content_type, sizeof out->content_type, "audio/wav");
    } else {
        out->audio = out_buf;
        snprintf(out->content_type, sizeof out->content_type, "audio/pcm; rate=%d; bits=16",
                 sample_rate);
    }
    out->ok = true;
    return true;
}
