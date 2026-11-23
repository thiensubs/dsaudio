// ds_http.h - HTTPS transport over libcurl. Buffered: a TTS reply is at most a
// few megabytes, so the whole body is collected before the caller sees it.
#ifndef DS_HTTP_H
#define DS_HTTP_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_http_response {
    long status;
    char *body;            /* malloc'd, NUL terminated; NULL when empty */
    size_t body_len;
    char content_type[160];
    char err[320];         /* transport failure, not an HTTP status */
} ds_http_response;

void ds_http_init(void);
void ds_http_response_free(ds_http_response *r);

bool ds_http_get(const char *url, const char *bearer, long timeout_s, ds_http_response *out);
bool ds_http_post_json(const char *url, const char *bearer, const char *json_body,
                       long timeout_s, ds_http_response *out);
bool ds_http_post_mem(const char *url, const char *bearer, const char *filename,
                      const void *data, size_t len, const char *field,
                      long timeout_s, ds_http_response *out);

#ifdef __cplusplus
}
#endif

#endif
