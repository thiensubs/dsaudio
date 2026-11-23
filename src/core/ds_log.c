#include "ds_log.h"

#include "ds_thread.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static ds_log_entry g_log[DS_LOG_MAX];
static int g_head = 0;      /* index of the next slot to write */
static int g_count = 0;
static ds_mutex *g_lock;

void ds_log_add(int level, const char *fmt, ...) {
    ds_log_entry e;
    e.t = ds_now_sec();
    e.level = level;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.text, sizeof e.text, fmt, ap);
    va_end(ap);

    if (!g_lock) g_lock = ds_mutex_new();
    ds_mutex_lock(g_lock);
    g_log[g_head] = e;
    g_head = (g_head + 1) % DS_LOG_MAX;
    if (g_count < DS_LOG_MAX) g_count++;
    ds_mutex_unlock(g_lock);
}

int ds_log_count(void) {
    if (!g_lock) g_lock = ds_mutex_new();
    ds_mutex_lock(g_lock);
    int n = g_count;
    ds_mutex_unlock(g_lock);
    return n;
}

const ds_log_entry *ds_log_at(int i) {
    if (i < 0) return NULL;
    if (!g_lock) g_lock = ds_mutex_new();
    ds_mutex_lock(g_lock);
    if (i >= g_count) {
        ds_mutex_unlock(g_lock);
        return NULL;
       }
    int idx = (g_head - g_count + i + DS_LOG_MAX * 2) % DS_LOG_MAX;
    ds_mutex_unlock(g_lock);
    return &g_log[idx];
}

void ds_log_clear(void) {
    if (!g_lock) g_lock = ds_mutex_new();
    ds_mutex_lock(g_lock);
    g_head = 0;
    g_count = 0;
    ds_mutex_unlock(g_lock);
}
