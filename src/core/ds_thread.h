// ds_thread.h - the two primitives the worker and the log need, on every
// platform: a mutex and a joinable thread. POSIX uses pthread, Windows uses
// CRITICAL_SECTION and CreateThread, so no consumer includes either header.
#ifndef DS_THREAD_H
#define DS_THREAD_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#include <windows.h>
typedef struct ds_mutex { CRITICAL_SECTION cs; } ds_mutex;
#else
typedef struct ds_mutex { struct pthread_mutex_t_ *m; } ds_mutex;
#endif

ds_mutex *ds_mutex_new(void);
void ds_mutex_free(ds_mutex *m);
void ds_mutex_lock(ds_mutex *m);
void ds_mutex_unlock(ds_mutex *m);

#if defined(_WIN32)
typedef struct ds_thread { void *h; unsigned long tid; } ds_thread;
#elif defined(__APPLE__)
#include <pthread.h>
typedef struct ds_thread { pthread_t t; _Bool started; } ds_thread;
#else
#include <pthread.h>
typedef struct ds_thread { pthread_t t; int started; } ds_thread;
#endif

/* Returns 0 on success, non-zero if the thread could not be created. */
int ds_thread_start(ds_thread *t, void *(*fn)(void *), void *arg);
void ds_thread_join(ds_thread *t);

#ifdef __cplusplus
}
#endif

#endif
