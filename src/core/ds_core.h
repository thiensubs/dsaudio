// ds_core.h - shared types for the dsaudio core (C API, C++ where simdjson needs it).
#ifndef DS_CORE_H
#define DS_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_buf {
    unsigned char *data;
    size_t len;
    size_t cap;
} ds_buf;

void ds_buf_init(ds_buf *b);
void ds_buf_free(ds_buf *b);
void ds_buf_reset(ds_buf *b);
bool ds_buf_reserve(ds_buf *b, size_t extra);
bool ds_buf_append(ds_buf *b, const void *p, size_t n);
bool ds_buf_append_str(ds_buf *b, const char *s);
bool ds_buf_append_byte(ds_buf *b, unsigned char c);
bool ds_buf_appendf(ds_buf *b, const char *fmt, ...);

typedef struct ds_err {
    char text[512];
} ds_err;

void ds_err_set(ds_err *e, const char *fmt, ...);
void ds_err_clear(ds_err *e);
static inline bool ds_err_ok(const ds_err *e) { return e->text[0] == '\0'; }

/* UTF-8 length in codepoints, and DeepSeek's billed length (a CJK character counts 2). */
size_t ds_utf8_len(const char *s);
size_t ds_utf8_billed_len(const char *s);

/* Monotonic seconds. */
double ds_now_sec(void);

/* localtime_r / localtime_s / localtime, one call. */
#include <time.h>
void ds_localtime(time_t t, struct tm *out);

/* fopen succeeds: no access()/unistd.h needed on Windows. */
bool ds_file_readable(const char *path);

/* The OS light/dark setting: true when dark. false when unknown. */
bool ds_system_dark(void);

/* Absolute path of the running executable, malloc'd, or NULL. */
char *ds_self_path(void);

#ifdef __cplusplus
}
#endif

#endif
