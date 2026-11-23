#include "ds_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd_u32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd_u16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint64_t rd_u64(const unsigned char *p) {
    return (uint64_t)rd_u32(p) | ((uint64_t)rd_u32(p + 4) << 32);
}

bool ds_audio_probe(const unsigned char *data, size_t len, ds_audio_info *info) {
    memset(info, 0, sizeof *info);
    if (!data || len < 12) return false;

    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        /* Not a container: DeepSeek's raw PCM is 16-bit mono 24 kHz. */
        info->sample_rate = 24000;
        info->channels = 1;
        info->bits = 16;
        info->is_raw_pcm = true;
        info->data_offset = 0;
        info->data_bytes = len;
        info->frames = len / 2;
        info->duration_s = (double)info->frames / 24000.0;
        return true;
    }

    info->is_wav = true;
    uint64_t pos = 12, end = len;
    int bytes_per_sample = 0;
    bool have_fmt = false;

    while (pos + 8 <= end) {
        const unsigned char *hdr = data + pos;
        uint32_t csize = rd_u32(hdr + 4);
        uint64_t body = pos + 8;
        if (body + csize > end) csize = (uint32_t)(end - body > 0 ? end - body : 0);

        if (memcmp(hdr, "fmt ", 4) == 0 && csize >= 16) {
            const unsigned char *f = data + body;
            int format = rd_u16(f);
            info->channels = rd_u16(f + 2);
            info->sample_rate = (int)rd_u32(f + 4);
            info->bits = rd_u16(f + 14);
            if (format == 3 && info->bits == 32) bytes_per_sample = 4;
            else bytes_per_sample = (info->bits / 8) * (info->channels ? info->channels : 1);
            have_fmt = true;
        } else if (memcmp(hdr, "data", 4) == 0) {
            info->data_offset = (int)body;
            info->data_bytes = csize;
        }
        pos = body + csize + (csize & 1);
    }

    if (!have_fmt || !info->sample_rate || bytes_per_sample <= 0) return false;
    info->frames = info->data_bytes / (uint64_t)bytes_per_sample;
    info->duration_s = (double)info->frames / (double)info->sample_rate;
    return true;
}

bool ds_audio_probe_file(const char *path, ds_audio_info *info) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char buf[8192];
    size_t got = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (got == 0) return false;
    return ds_audio_probe(buf, got, info);
}

ds_audio_info ds_audio_raw_pcm_info(uint64_t bytes) {
    ds_audio_info info;
    memset(&info, 0, sizeof info);
    info.sample_rate = 24000;
    info.channels = 1;
    info.bits = 16;
    info.is_raw_pcm = true;
    info.data_offset = 0;
    info.data_bytes = bytes;
    info.frames = bytes / 2;
    info.duration_s = (double)info.frames / 24000.0;
    return info;
}

int ds_audio_peaks(const unsigned char *data, size_t len, const ds_audio_info *info,
                   float *peaks, int count) {
    if (!data || !info || !peaks || count <= 0) return 0;
    int bytes_per_frame = (info->bits / 8) * (info->channels ? info->channels : 1);
    if (bytes_per_frame <= 0) return 0;

    uint64_t data_off = (uint64_t)info->data_offset;
    if (data_off >= len) return 0;
    uint64_t avail = len - data_off;
    uint64_t frames = info->frames;
    if (frames == 0) frames = avail / (uint64_t)bytes_per_frame;
    if (frames == 0) return 0;

    uint64_t per = frames / (uint64_t)count;
    if (per == 0) per = 1;

    for (int i = 0; i < count; i++) {
        uint64_t start = (uint64_t)i * per;
        uint64_t stop = start + per;
        if (start >= frames) break;
        if (stop > frames) stop = frames;

        double peak = 0.0;
        for (uint64_t fr = start; fr < stop; fr++) {
            uint64_t off = data_off + fr * (uint64_t)bytes_per_frame;
            if (off + (uint64_t)bytes_per_frame > len) break;
            const unsigned char *s = data + off;
            double v = 0.0;
            if (info->bits == 16) {
                int16_t x = (int16_t)rd_u16(s);
                v = (double)x / 32768.0;
            } else if (info->bits == 8) {
                v = ((double)s[0] - 128.0) / 128.0;
            } else if (info->bits == 24) {
                int32_t x = (int32_t)((uint32_t)s[0] | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16));
                if (x & 0x800000) x |= (int32_t)0xFF000000;
                v = (double)x / 8388608.0;
            } else if (info->bits == 32) {
                v = (double)*(float *)(void *)s;
            }
            double a = v < 0 ? -v : v;
            if (a > peak) peak = a;
        }
        peaks[i] = (float)(peak > 1.0 ? 1.0 : peak);
    }
    return count;
}
