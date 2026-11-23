// ds_media.h - turn whatever the user has (MP3, FLAC, Ogg, WAV) into the
// 16-bit PCM WAV that DeepSeek ASR accepts. miniaudio does the decoding; it is
// already linked for playback, so this adds no dependency.
#ifndef DS_MEDIA_H
#define DS_MEDIA_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* True when the bytes are already a container ASR takes: RIFF/WAVE or Ogg. */
bool ds_media_is_asr_ready(const unsigned char *data, size_t len);

/* Decode an audio file to a 16-bit PCM WAV in memory. Returns false and fills
 * err when the file cannot be decoded. */
bool ds_media_decode_to_wav(const char *path, ds_buf *wav, char *err, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
