// ds_job.h - one HTTP job at a time, on a worker thread, so the UI thread never
// blocks. The UI starts a job, keeps drawing, and harvests the result.
#ifndef DS_JOB_H
#define DS_JOB_H

#include "ds_account.h"
#include "ds_asr.h"
#include "ds_config.h"
#include "ds_core.h"
#include "ds_tts.h"

#include "ds_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ds_job_kind { DS_JOB_NONE = 0, DS_JOB_TTS, DS_JOB_ASR, DS_JOB_BALANCE } ds_job_kind;

typedef struct ds_job {
    ds_job_kind kind;
    ds_config cfg;

    char text[16384];
    char voice[16];
    char format[16];
    bool filter_markdown;
    char path[512];
    long timeout_s;

    bool running;
    bool done;
    double elapsed;
    ds_tts_result tts;
    ds_tts_artifact artifact;
    ds_asr_result asr;
    ds_balance balance;

    ds_thread thread;
    ds_mutex *mutex;
} ds_job;

void ds_job_init(ds_job *j);
void ds_job_shutdown(ds_job *j);

bool ds_job_start_tts(ds_job *j, const ds_config *cfg, const char *text,
                      const char *voice, const char *format, bool filter_markdown);
bool ds_job_start_asr(ds_job *j, const ds_config *cfg, const char *path);
bool ds_job_start_balance(ds_job *j, const ds_config *cfg);

bool ds_job_is_running(const ds_job *j);
bool ds_job_poll(ds_job *j);    /* true when a finished job is ready to harvest */
void ds_job_harvest(ds_job *j); /* join the thread and clear the slot */

#ifdef __cplusplus
}
#endif

#endif
