// ds_audio.h - read a WAV (or raw PCM) without a dependency, and reduce it to
// the peaks a waveform draws.
#ifndef DS_AUDIO_H
#define DS_AUDIO_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_audio_info {
    int sample_rate;
    int channels;
    int bits;
    uint64_t frames;
    double duration_s;
    int data_offset;      /* byte offset of the sample data in the file */
    uint64_t data_bytes;
    bool is_wav;
    bool is_raw_pcm;
} ds_audio_info;

/* Reads the header only; data points into the buffer, which must outlive the info. */
bool ds_audio_probe(const unsigned char *data, size_t len, ds_audio_info *info);
bool ds_audio_probe_file(const char *path, ds_audio_info *info);

/* Reduce PCM sample data to `count` peaks in 0..1. Returns the number written. */
int ds_audio_peaks(const unsigned char *data, size_t len, const ds_audio_info *info,
                   float *peaks, int count);

/* DeepSeek's raw PCM format. */
ds_audio_info ds_audio_raw_pcm_info(uint64_t bytes);

#ifdef __cplusplus
}
#endif

#endif
