#include "ds_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <winreg.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

void ds_buf_init(ds_buf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void ds_buf_free(ds_buf *b) {
    free(b->data);
    ds_buf_init(b);
}

void ds_buf_reset(ds_buf *b) {
    b->len = 0;
    if (b->data) b->data[0] = '\0';
}

bool ds_buf_reserve(ds_buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return true;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + extra + 1) cap *= 2;
    unsigned char *p = (unsigned char *)realloc(b->data, cap);
    if (!p) return false;
    b->data = p;
    b->cap = cap;
    return true;
}

bool ds_buf_append(ds_buf *b, const void *p, size_t n) {
    if (n == 0) return true;
    if (!ds_buf_reserve(b, n)) return false;
    memcpy(b->data + b->len, p, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

bool ds_buf_append_str(ds_buf *b, const char *s) {
    return ds_buf_append(b, s, strlen(s));
}

bool ds_buf_append_byte(ds_buf *b, unsigned char c) {
    return ds_buf_append(b, &c, 1);
}

bool ds_buf_appendf(ds_buf *b, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return false;
    if ((size_t)n < sizeof tmp) return ds_buf_append(b, tmp, (size_t)n);
    char *big = (char *)malloc((size_t)n + 1);
    if (!big) return false;
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    bool ok = ds_buf_append(b, big, (size_t)n);
    free(big);
    return ok;
}

void ds_err_set(ds_err *e, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->text, sizeof e->text, fmt, ap);
    va_end(ap);
}

void ds_err_clear(ds_err *e) {
    e->text[0] = '\0';
}

bool ds_system_dark(void) {
#if defined(__APPLE__)
    CFStringRef v = (CFStringRef)CFPreferencesCopyValue(
        CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication,
        kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if (!v) return false;               /* the key only exists in dark mode */
    bool dark = CFStringCompare(v, CFSTR("Dark"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
    CFRelease(v);
    return dark;
#elif defined(_WIN32)
    DWORD v = 1, sz = sizeof v;
    if (RegGetValueA(HKEY_CURRENT_USER,
                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     "AppsUseLightTheme", RRF_RT_DWORD, NULL, &v, &sz) != ERROR_SUCCESS)
        return false;
    return v == 0;
#else
    const char *gtk = getenv("GTK_THEME");
    if (gtk && strstr(gtk, ":dark")) return true;
    return false;
#endif
}

char *ds_self_path(void) {
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
    char *out = (char *)malloc(strlen(buf) + 1);
    if (out) strcpy(out, buf);
    return out;
}

double ds_now_sec(void) {
#if defined(_WIN32)
    /* MSVC has no clock_gettime and no CLOCK_MONOTONIC. */
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

/* --- UTF-8 --------------------------------------------------------------- */

static size_t utf8_seq_len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; /* continuation byte or junk: count it as one */
}

static uint32_t utf8_decode(const unsigned char *s, size_t n) {
    if (n == 1) return s[0];
    if (n == 2) return ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    if (n == 3) return ((uint32_t)(s[0] & 0x0F) << 12)
                     | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    return ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12)
         | ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

size_t ds_utf8_len(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = 0;
    while (*p) {
        size_t k = utf8_seq_len(*p);
        p += k;
        n++;
    }
    return n;
}

static bool is_cjk(uint32_t cp) {
    return (cp >= 0x2E80 && cp <= 0x9FFF)      /* CJK radicals .. unified ideographs */
        || (cp >= 0xAC00 && cp <= 0xD7AF)      /* hangul syllables */
        || (cp >= 0xF900 && cp <= 0xFAFF)      /* compatibility ideographs */
        || (cp >= 0x3040 && cp <= 0x30FF)      /* hiragana, katakana */
        || (cp >= 0x3400 && cp <= 0x4DBF)      /* CJK extension A */
        || (cp >= 0x20000 && cp <= 0x2FA1F);   /* CJK extensions B..F */
}

size_t ds_utf8_billed_len(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t billed = 0;
    while (*p) {
        size_t k = utf8_seq_len(*p);
        uint32_t cp = utf8_decode(p, k);
        billed += is_cjk(cp) ? 2 : 1;
        p += k;
    }
    return billed;
}
