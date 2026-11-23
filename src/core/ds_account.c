#include "ds_account.h"

#include "ds_http.h"
#include "ds_json.h"

#include <stdio.h>
#include <string.h>

static void extract_first_balance(const char *raw, ds_balance *out) {
    /* The reply is {"is_available":...,"balance_infos":[{...}]}. Take the first
     * object inside the array and read it with the same simdjson helpers. */
    const char *open = strchr(raw, '[');
    if (!open) return;
    const char *brace = strchr(open, '{');
    if (!brace) return;
    /* A balance entry has no nested objects, so the first '}' closes it. */
    const char *end = strchr(brace, '}');
    if (!end) return;

    ds_json *e = ds_json_parse(brace, (size_t)(end - brace + 1));
    if (!e) return;
    ds_json_str(e, "currency", out->currency, sizeof out->currency);
    ds_json_str(e, "total_balance", out->total, sizeof out->total);
    ds_json_str(e, "granted_balance", out->granted, sizeof out->granted);
    ds_json_str(e, "topped_up_balance", out->topped_up, sizeof out->topped_up);
    ds_json_free(e);
}

bool ds_balance_query(const ds_config *cfg, long timeout_s, ds_balance *out) {
    memset(out, 0, sizeof *out);
    out->ok = false;

    if (!cfg || !cfg->have_key) {
        snprintf(out->err, sizeof out->err, "no DeepSeek key");
        return false;
    }

    char url[512];
    snprintf(url, sizeof url, "%s/user/balance", cfg->base_url);

    ds_http_response resp;
    if (!ds_http_get(url, cfg->api_key, timeout_s > 0 ? timeout_s : 20, &resp)) {
        snprintf(out->err, sizeof out->err, "%s", resp.err);
        ds_http_response_free(&resp);
        return false;
    }
    if (resp.status < 200 || resp.status >= 300) {
        snprintf(out->err, sizeof out->err, "HTTP %ld: %.200s", resp.status,
                 resp.body ? resp.body : "");
        ds_http_response_free(&resp);
        return false;
    }

    ds_json *j = ds_json_parse(resp.body ? resp.body : "", resp.body_len);
    if (!j) {
        snprintf(out->err, sizeof out->err, "the reply was not JSON");
        ds_http_response_free(&resp);
        return false;
    }
    ds_json_bool(j, "is_available", &out->is_available);
    ds_json_free(j);
    if (resp.body) extract_first_balance(resp.body, out);

    ds_http_response_free(&resp);
    out->ok = true;
    return true;
}
