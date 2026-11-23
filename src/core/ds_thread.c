#include "ds_thread.h"

#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif


ds_mutex *ds_mutex_new(void) {
    ds_mutex *m = (ds_mutex *)calloc(1, sizeof *m);
    if (!m) return NULL;
#if defined(_WIN32)
    InitializeCriticalSection(&m->cs);
#else
    pthread_mutex_init(&m->m, NULL);
#endif
    return m;
}

void ds_mutex_free(ds_mutex *m) {
    if (!m) return;
#if defined(_WIN32)
    DeleteCriticalSection(&m->cs);
#else
    pthread_mutex_destroy(&m->m);
#endif
    free(m);
}

void ds_mutex_lock(ds_mutex *m) {
    if (!m) return;
#if defined(_WIN32)
    EnterCriticalSection(&m->cs);
#else
    pthread_mutex_lock(&m->m);
#endif
}

void ds_mutex_unlock(ds_mutex *m) {
    if (!m) return;
#if defined(_WIN32)
    LeaveCriticalSection(&m->cs);
#else
    pthread_mutex_unlock(&m->m);
#endif
}

#if defined(_WIN32)

struct ds_win_arg { void *(*fn)(void *); void *arg; };

static DWORD WINAPI win_trampoline(LPVOID p) {
    struct ds_win_arg *w = (struct ds_win_arg *)p;
    void *(*fn)(void *) = w->fn;
    void *arg = w->arg;
    free(w);
    return (DWORD)(uintptr_t)fn(arg);
}

int ds_thread_start(ds_thread *t, void *(*fn)(void *), void *arg) {
    struct ds_win_arg *w = (struct ds_win_arg *)malloc(sizeof *w);
    if (!w) return -1;
    w->fn = fn;
    w->arg = arg;
    t->h = CreateThread(NULL, 0, win_trampoline, w, 0, &t->tid);
    return t->h ? 0 : -1;
}

void ds_thread_join(ds_thread *t) {
    if (t->h) {
        WaitForSingleObject(t->h, INFINITE);
        CloseHandle(t->h);
        t->h = NULL;
    }
}

#else

int ds_thread_start(ds_thread *t, void *(*fn)(void *), void *arg) {
    t->started = pthread_create(&t->t, NULL, fn, arg) == 0;
    return t->started ? 0 : -1;
}

void ds_thread_join(ds_thread *t) {
    if (t->started) {
        pthread_join(t->t, NULL);
        t->started = false;
    }
}

#endif
