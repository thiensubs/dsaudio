// ds_log.h - a small thread-safe ring buffer, so a worker thread can report
// what it did while the UI only reads.
#ifndef DS_LOG_H
#define DS_LOG_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { DS_LOG_INFO = 0, DS_LOG_OK = 1, DS_LOG_WARN = 2, DS_LOG_ERROR = 3 };
enum { DS_LOG_MAX = 256 };

typedef struct ds_log_entry {
    double t;
    int level;
    char text[256];
} ds_log_entry;

void ds_log_add(int level, const char *fmt, ...);
int ds_log_count(void);
const ds_log_entry *ds_log_at(int i);   /* 0 is the oldest retained entry */
void ds_log_clear(void);

#ifdef __cplusplus
}
#endif

#endif
