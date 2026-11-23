// ds_json.h - simdjson-backed JSON read/write with a C API.
#ifndef DS_JSON_H
#define DS_JSON_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_json ds_json;

ds_json *ds_json_parse(const char *text, size_t len);
void ds_json_free(ds_json *j);

bool ds_json_str(ds_json *j, const char *key, char *out, size_t cap);
bool ds_json_i64(ds_json *j, const char *key, int64_t *out);
bool ds_json_bool(ds_json *j, const char *key, bool *out);
bool ds_json_is_object(ds_json *j);

/* Dotted path into nested objects, e.g. "params.arguments.text". */
bool ds_json_at_str(ds_json *j, const char *path, char *out, size_t cap);
bool ds_json_at_i64(ds_json *j, const char *path, int64_t *out);
bool ds_json_at_bool(ds_json *j, const char *path, bool *out);
/* The raw JSON text of a member, quotes included for strings: "id" stays a
 * JSON string or number in the reply. */
bool ds_json_raw(ds_json *j, const char *path, char *out, size_t cap);

/* Object writer: ds_jsonw_finish returns a malloc'd string and frees the writer. */
typedef struct ds_jsonw ds_jsonw;

ds_jsonw *ds_jsonw_new(void);
void ds_jsonw_free(ds_jsonw *w);
void ds_jsonw_set_str(ds_jsonw *w, const char *key, const char *val);
void ds_jsonw_set_i64(ds_jsonw *w, const char *key, int64_t val);
void ds_jsonw_set_bool(ds_jsonw *w, const char *key, bool val);
char *ds_jsonw_finish(ds_jsonw *w);

#ifdef __cplusplus
}
#endif

#endif
