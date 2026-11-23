#include "ds_http.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ds_http_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void ds_http_response_free(ds_http_response *r) {
    if (!r) return;
    free(r->body);
    r->body = NULL;
    r->body_len = 0;
    r->status = 0;
    r->content_type[0] = '\0';
    r->err[0] = '\0';
}

static bool set_err(ds_http_response *out, const char *what, CURLcode rc) {
    snprintf(out->err, sizeof out->err, "%s: %s", what, curl_easy_strerror(rc));
    return false;
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *user) {
    ds_buf *b = (ds_buf *)user;
    size_t n = size * nmemb;
    if (!ds_buf_append(b, ptr, n)) return 0;   /* abort the transfer */
    return n;
}

static bool perform(CURL *curl, ds_http_response *out) {
    ds_buf body;
    ds_buf_init(&body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        ds_buf_free(&body);
        return set_err(out, "request failed", rc);
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    char *ctype = NULL;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ctype);
    if (ctype) snprintf(out->content_type, sizeof out->content_type, "%s", ctype);

    out->status = status;
    out->body_len = body.len;
    out->body = (char *)body.data;   /* already NUL terminated by ds_buf_append */
    if (!out->body) {
        out->body = (char *)malloc(1);
        if (out->body) out->body[0] = '\0';
    }
    return true;
}

bool ds_http_post_json(const char *url, const char *bearer, const char *json_body,
                       long timeout_s, ds_http_response *out) {
    out->status = 0;
    out->body = NULL;
    out->body_len = 0;
    out->content_type[0] = '\0';
    out->err[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) return set_err(out, "curl_easy_init", CURLE_FAILED_INIT);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (bearer && bearer[0]) {
        char auth[640];
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", bearer);
        headers = curl_slist_append(headers, auth);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);

    bool ok = perform(curl, out);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ok;
}

bool ds_http_post_mem(const char *url, const char *bearer, const char *filename,
                      const void *data, size_t len, const char *field,
                      long timeout_s, ds_http_response *out) {
    out->status = 0;
    out->body = NULL;
    out->body_len = 0;
    out->content_type[0] = '\0';
    out->err[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) return set_err(out, "curl_easy_init", CURLE_FAILED_INIT);

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_name(part, field);
    curl_mime_filename(part, filename);
    curl_mime_data(part, (const char *)data, (curl_off_t)len);
    curl_mime_type(part, "application/octet-stream");

    struct curl_slist *headers = NULL;
    if (bearer && bearer[0]) {
        char auth[640];
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", bearer);
        headers = curl_slist_append(headers, auth);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);

    bool ok = perform(curl, out);
    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    return ok;
}

bool ds_http_get(const char *url, const char *bearer, long timeout_s, ds_http_response *out) {
    out->status = 0;
    out->body = NULL;
    out->body_len = 0;
    out->content_type[0] = '\0';
    out->err[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) return set_err(out, "curl_easy_init", CURLE_FAILED_INIT);

    struct curl_slist *headers = NULL;
    if (bearer && bearer[0]) {
        char auth[640];
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", bearer);
        headers = curl_slist_append(headers, auth);
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);

    bool ok = perform(curl, out);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ok;
}
