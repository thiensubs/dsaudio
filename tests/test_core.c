// test_core.c - offline tests for the pure logic. No network, no key.
//
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
//   cmake --build build --target dsaudio-test
//   ctest --test-dir build --output-on-failure
#include "../src/core/ds_audio.h"
#include "../src/core/ds_json.h"
#include "../src/core/ds_media.h"
#include "../src/core/ds_tts_cache.h"
#include "../src/core/ds_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                   printf("\n"); g_fail++; } \
} while (0)

static void test_utf8(void) {
    CHECK(ds_utf8_len("hello") == 5, "ascii len");
    CHECK(ds_utf8_len("Ti\u1ebfng Vi\u1ec7t") == 10, "vietnamese len, got %zu",
          ds_utf8_len("Ti\u1ebfng Vi\u1ec7t"));
    CHECK(ds_utf8_billed_len("abc") == 3, "ascii billed");
    CHECK(ds_utf8_billed_len("\u4e2d\u6587") == 4, "two CJK chars bill 4, got %zu",
          ds_utf8_billed_len("\u4e2d\u6587"));
    CHECK(ds_utf8_billed_len("a\u4e2d") == 3, "mixed billed");
}

static void test_json(void) {
    ds_jsonw *w = ds_jsonw_new();
    ds_jsonw_set_str(w, "text", "a \"quote\" and a \n newline \u4e2d");
    ds_jsonw_set_i64(w, "duration_ms", 2960);
    ds_jsonw_set_bool(w, "ok", true);
    char *body = ds_jsonw_finish(w);

    ds_json *j = ds_json_parse(body, strlen(body));
    CHECK(j != NULL, "parse built object");
    if (!j) { free(body); return; }
    char s[256];
    int64_t n = 0;
    bool b = false;
    CHECK(ds_json_str(j, "text", s, sizeof s), "read string");
    CHECK(strcmp(s, "a \"quote\" and a \n newline \u4e2d") == 0, "string round trip: %s", s);
    CHECK(ds_json_i64(j, "duration_ms", &n) && n == 2960, "int round trip");
    CHECK(ds_json_bool(j, "ok", &b) && b, "bool round trip");
    CHECK(ds_json_str(j, "missing", s, sizeof s) == false, "missing key fails");
    ds_json_free(j);

    ds_json *bad = ds_json_parse("{\"a\":", 5);
    CHECK(bad == NULL, "truncated JSON is rejected");
    ds_json_free(bad);
    free(body);
}

static void test_audio(void) {
    /* a 1 kHz-ish sine in a 24 kHz mono 16-bit WAV */
    const int rate = 24000, frames = 4800;   /* 0.2 s of a 50 Hz sine */
    ds_buf wav;
    ds_buf_init(&wav);
    unsigned char h[44] = {0};   /* zeroed: an unset byte is not a field */
    memcpy(h, "RIFF", 4);
    uint32_t riff = 36 + (uint32_t)frames * 2;
    h[4] = (unsigned char)riff; h[5] = (unsigned char)(riff >> 8);
    h[6] = (unsigned char)(riff >> 16); h[7] = (unsigned char)(riff >> 24);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = 1; h[22] = 1;
    h[24] = (unsigned char)rate; h[25] = (unsigned char)(rate >> 8);
    h[26] = (unsigned char)(rate >> 16); h[27] = (unsigned char)(rate >> 24);
    h[34] = 16; h[35] = 0;               /* bits per sample, little endian */
    memcpy(h + 36, "data", 4);
    h[40] = (unsigned char)(frames * 2); h[41] = (unsigned char)(frames * 2 >> 8);
    ds_buf_append(&wav, h, sizeof h);
    for (int i = 0; i < frames; i++) {
        short v = (short)(20000.0 * sin(2.0 * 3.14159265358979 * 50.0 * (double)i / rate));
        ds_buf_append(&wav, &v, 2);
    }

    ds_audio_info info;
    CHECK(ds_audio_probe(wav.data, wav.len, &info), "probe a synthetic WAV");
    CHECK(info.is_wav && info.sample_rate == rate && info.channels == 1 && info.bits == 16,
          "WAV fields: %d Hz %d ch %d bit", info.sample_rate, info.channels, info.bits);
    CHECK(info.frames == (uint64_t)frames, "frame count, got %llu",
          (unsigned long long)info.frames);
    CHECK(info.duration_s > 0.19 && info.duration_s < 0.21, "duration %.3f", info.duration_s);

    float peaks[64];
    int n = ds_audio_peaks(wav.data, wav.len, &info, peaks, 64);
    CHECK(n == 64, "peak count, got %d", n);
    float lo = 2.0f, hi = 0.0f;
    for (int i = 0; i < n; i++) {
        if (peaks[i] < lo) lo = peaks[i];
        if (peaks[i] > hi) hi = peaks[i];
    }
    CHECK(hi > 0.25f, "a 50 Hz sine must produce real peaks, max %.3f", hi);
    CHECK(lo < hi * 0.75f, "peaks must vary along the waveform: min %.3f max %.3f", lo, hi);

    ds_buf_free(&wav);
}

static void test_media(void) {
    unsigned char wav[12] = { 'R','I','F','F',0,0,0,0,'W','A','V','E' };
    unsigned char ogg[12] = { 'O','g','g','S',0,0,0,0,0,0,0,0 };
    unsigned char mp3[3] = { 0xFF, 0xFB, 0x90 };
    CHECK(ds_media_is_asr_ready(wav, sizeof wav), "WAV is ASR ready");
    CHECK(ds_media_is_asr_ready(ogg, sizeof ogg), "Ogg is ASR ready");
    CHECK(!ds_media_is_asr_ready(mp3, sizeof mp3), "MP3 must be converted first");
}

static void test_split(void) {
    const char *t = "One. Two! Three? Four five six seven.";
    int starts[32], lens[32];
    int n = ds_tts_split(t, 12, starts, lens, 32);
    CHECK(n >= 3, "splits into sentences, got %d", n);
    int total = 0;
    for (int i = 0; i < n; i++) {
        CHECK(lens[i] > 0, "chunk %d is not empty", i);
        total += lens[i];
    }
    CHECK(total <= (int)strlen(t), "chunks do not exceed the text: %d vs %zu", total, strlen(t));

    /* a single long word must still produce exactly one chunk, not a loop */
    char big[512];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    int n2 = ds_tts_split(big, 64, starts, lens, 32);
    CHECK(n2 == 8, "a 511-char run splits into 8 chunks, got %d", n2);
}

int main(void) {
    test_utf8();
    test_json();
    test_audio();
    test_media();
    test_split();
    if (g_fail) {
        printf("%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("all core checks passed\n");
    return 0;
}
