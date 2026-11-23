#include "ds_media.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniaudio.h"

static void put_u32(ds_buf *b, uint32_t v) {
    unsigned char p[4] = { (unsigned char)v, (unsigned char)(v >> 8),
                           (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    ds_buf_append(b, p, 4);
}

static void put_u16(ds_buf *b, uint16_t v) {
    unsigned char p[2] = { (unsigned char)v, (unsigned char)(v >> 8) };
    ds_buf_append(b, p, 2);
}

bool ds_media_is_asr_ready(const unsigned char *data, size_t len) {
    if (!data || len < 12) return false;
    if (memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0) return true;
    if (memcmp(data, "OggS", 4) == 0) return true;
    if (memcmp(data, "fLaC", 4) == 0) return false;   /* decode it */
    if (data[0] == 0xFF && (data[1] & 0xE0) == 0xE0) return false;  /* MP3 */
    return false;
}

bool ds_media_decode_to_wav(const char *path, ds_buf *wav, char *err, size_t cap) {
    ds_buf_init(wav);
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, 0, 0);
    ma_decoder dec;
    if (ma_decoder_init_file(path, &cfg, &dec) != MA_SUCCESS) {
        snprintf(err, cap, "cannot decode %s as MP3, FLAC, Ogg or WAV", path);
        return false;
    }

    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames(&dec, &frames);
    ma_uint32 channels = dec.outputChannels ? dec.outputChannels : 1;
    ma_uint32 rate = dec.outputSampleRate ? dec.outputSampleRate : 24000;
    size_t samples = (size_t)frames * (size_t)channels;
    short *pcm = (short *)malloc(samples * sizeof(short));
    if (!pcm) {
        ma_decoder_uninit(&dec);
        snprintf(err, cap, "out of memory for %llu frames", (unsigned long long)frames);
        return false;
    }
    ma_uint64 got = 0;
    if (ma_decoder_read_pcm_frames(&dec, pcm, frames, &got) != MA_SUCCESS) {
        free(pcm);
        ma_decoder_uninit(&dec);
        snprintf(err, cap, "decode failed: %s", path);
        return false;
    }
    ma_decoder_uninit(&dec);

    uint64_t data_bytes = (uint64_t)got * (uint64_t)channels * 2u;
    uint32_t riff = (uint32_t)(36u + data_bytes);
    ds_buf_append(wav, "RIFF", 4);
    put_u32(wav, riff);
    ds_buf_append(wav, "WAVE", 4);
    ds_buf_append(wav, "fmt ", 4);
    put_u32(wav, 16);
    put_u16(wav, 1);                 /* PCM */
    put_u16(wav, (uint16_t)channels);
    put_u32(wav, rate);
    put_u32(wav, rate * channels * 2u);
    put_u16(wav, (uint16_t)(channels * 2u));
    put_u16(wav, 16);
    ds_buf_append(wav, "data", 4);
    put_u32(wav, (uint32_t)data_bytes);
    ds_buf_append(wav, pcm, (size_t)data_bytes);
    free(pcm);
    if (!wav->data) {
        snprintf(err, cap, "out of memory writing the WAV");
        return false;
    }
    return true;
}
