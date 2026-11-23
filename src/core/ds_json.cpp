// ds_json.cpp - simdjson (vendored single header) behind a small C API.
#include "ds_json.h"

#include "simdjson.h"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace simdjson;

struct ds_json {
    std::string buf;
    dom::parser parser;
    dom::element doc;
    bool ok = false;
};

ds_json *ds_json_parse(const char *text, size_t len) {
    if (!text) return NULL;
    ds_json *j = new ds_json();
    j->buf.assign(text, len);
    if (j->buf.capacity() < j->buf.size() + SIMDJSON_PADDING)
        j->buf.reserve(j->buf.size() + SIMDJSON_PADDING);
    if (j->parser.parse(j->buf).get(j->doc)) {
        delete j;
        return NULL;
    }
    j->ok = true;
    return j;
}

void ds_json_free(ds_json *j) {
    delete j;
}

bool ds_json_is_object(ds_json *j) {
    if (!j || !j->ok) return false;
    dom::object o;
    return j->doc.get_object().get(o) == SUCCESS;
}

bool ds_json_str(ds_json *j, const char *key, char *out, size_t cap) {
    if (!j || !j->ok || !key || !out || cap == 0) return false;
    out[0] = '\0';
    dom::object o;
    if (j->doc.get_object().get(o)) return false;
    std::string_view v;
    if (o[key].get_string().get(v)) return false;
    size_t n = v.size() < cap - 1 ? v.size() : cap - 1;
    memcpy(out, v.data(), n);
    out[n] = '\0';
    return true;
}

bool ds_json_i64(ds_json *j, const char *key, int64_t *out) {
    if (!j || !j->ok || !key || !out) return false;
    dom::object o;
    if (j->doc.get_object().get(o)) return false;
    return o[key].get_int64().get(*out) == SUCCESS;
}

bool ds_json_bool(ds_json *j, const char *key, bool *out) {
    if (!j || !j->ok || !key || !out) return false;
    dom::object o;
    if (j->doc.get_object().get(o)) return false;
    return o[key].get_bool().get(*out) == SUCCESS;
}

/* --- nested paths -------------------------------------------------------- */

static bool walk(const dom::element &root, const char *path, dom::element &out) {
    dom::element cur = root;
    const char *p = path;
    while (p && *p) {
        const char *dot = strchr(p, '.');
        size_t klen = dot ? (size_t)(dot - p) : strlen(p);
        if (klen == 0) return false;
        dom::object obj;
        if (cur.get_object().get(obj)) return false;
        dom::element next;
        if (obj.at_key(std::string_view(p, klen)).get(next)) return false;
        cur = next;
        if (!dot) break;
        p = dot + 1;
    }
    out = cur;
    return true;
}

static bool reach(ds_json *j, const char *path, dom::element &e) {
    if (!j || !j->ok || !path) return false;
    return walk(j->doc, path, e);
}

bool ds_json_at_str(ds_json *j, const char *path, char *out, size_t cap) {
    if (!out || cap == 0) return false;
    out[0] = '\0';
    dom::element e;
    if (!reach(j, path, e)) return false;
    std::string_view v;
    if (e.get_string().get(v)) return false;
    size_t n = v.size() < cap - 1 ? v.size() : cap - 1;
    memcpy(out, v.data(), n);
    out[n] = '\0';
    return true;
}

bool ds_json_at_i64(ds_json *j, const char *path, int64_t *out) {
    if (!out) return false;
    dom::element e;
    if (!reach(j, path, e)) return false;
    return e.get_int64().get(*out) == SUCCESS;
}

bool ds_json_at_bool(ds_json *j, const char *path, bool *out) {
    if (!out) return false;
    dom::element e;
    if (!reach(j, path, e)) return false;
    return e.get_bool().get(*out) == SUCCESS;
}

bool ds_json_raw(ds_json *j, const char *path, char *out, size_t cap) {
    if (!out || cap == 0) return false;
    out[0] = '\0';
    dom::element e;
    if (!reach(j, path, e)) return false;
    if (e.is_null()) return true;
    if (e.is_string()) {
        std::string_view v;
        if (e.get_string().get(v)) return false;
        size_t n = v.size() < cap - 3 ? v.size() : cap - 3;
        snprintf(out, cap, "\"%.*s\"", (int)n, v.data());
        return true;
    }
    if (e.is_int64()) {
        int64_t v;
        if (e.get_int64().get(v)) return false;
        snprintf(out, cap, "%lld", (long long)v);
        return true;
    }
    if (e.is_bool()) {
        bool v;
        if (e.get_bool().get(v)) return false;
        snprintf(out, cap, "%s", v ? "true" : "false");
        return true;
    }
    snprintf(out, cap, "null");
    return true;
}

/* --- writer -------------------------------------------------------------- */

struct ds_jsonw {
    std::string out;
    bool first = true;
};

static void append_escaped(std::string &out, const char *s) {
    out.push_back('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        default:
            if (*p < 0x20) {
                char esc[8];
                snprintf(esc, sizeof esc, "\\u%04x", (unsigned)*p);
                out += esc;
            } else {
                out.push_back((char)*p);
            }
        }
    }
    out.push_back('"');
}

ds_jsonw *ds_jsonw_new(void) {
    ds_jsonw *w = new ds_jsonw();
    w->out = "{";
    return w;
}

void ds_jsonw_free(ds_jsonw *w) {
    delete w;
}

static void begin_member(ds_jsonw *w) {
    if (!w->first) w->out.push_back(',');
    w->first = false;
}

void ds_jsonw_set_str(ds_jsonw *w, const char *key, const char *val) {
    if (!w || !key) return;
    begin_member(w);
    append_escaped(w->out, key);
    w->out.push_back(':');
    append_escaped(w->out, val);
}

void ds_jsonw_set_i64(ds_jsonw *w, const char *key, int64_t val) {
    if (!w || !key) return;
    begin_member(w);
    append_escaped(w->out, key);
    w->out.push_back(':');
    w->out += std::to_string(val);
}

void ds_jsonw_set_bool(ds_jsonw *w, const char *key, bool val) {
    if (!w || !key) return;
    begin_member(w);
    append_escaped(w->out, key);
    w->out.push_back(':');
    w->out += val ? "true" : "false";
}

char *ds_jsonw_finish(ds_jsonw *w) {
    if (!w) return NULL;
    w->out.push_back('}');
    char *s = (char *)malloc(w->out.size() + 1);
    if (s) memcpy(s, w->out.data(), w->out.size() + 1);
    delete w;
    return s;
}
