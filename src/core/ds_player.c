#include "ds_player.h"

#include <string.h>

/* ma_engine owns the playback device and the resource manager, and ma_sound
 * streams the file: opening a clip does not decode it, so the UI thread never
 * waits on the audio. */
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

struct ds_player {
    ma_engine engine;
    ma_sound sound;
    bool engine_ready;
    bool sound_ready;
};

ds_player *ds_player_create(void) {
    return (ds_player *)calloc(1, sizeof(ds_player));
}

void ds_player_destroy(ds_player *p) {
    if (!p) return;
    ds_player_close(p);
    free(p);
}

bool ds_player_open(ds_player *p, const char *path, char *err, size_t cap) {
    if (!p) return false;
    ds_player_close(p);
    if (!path || !path[0]) {
        if (err) snprintf(err, cap, "no file");
        return false;
    }

    if (!p->engine_ready) {
        ma_engine_config ec = ma_engine_config_init();
        ec.pResourceManager = NULL;
        if (ma_engine_init(&ec, &p->engine) != MA_SUCCESS) {
            if (err) snprintf(err, cap, "cannot open the audio device");
            return false;
        }
        p->engine_ready = true;
    }

    /* STREAM keeps open fast; the device reads from the file as it plays. */
    if (ma_sound_init_from_file(&p->engine, path, MA_SOUND_FLAG_STREAM, NULL, NULL, &p->sound) != MA_SUCCESS) {
        if (err) snprintf(err, cap, "cannot decode %s (WAV, not PCM)", path);
        return false;
    }
    p->sound_ready = true;
    return true;
}

void ds_player_close(ds_player *p) {
    if (!p) return;
    if (p->sound_ready) {
        ma_sound_uninit(&p->sound);
        p->sound_ready = false;
    }
    if (p->engine_ready) {
        ma_engine_uninit(&p->engine);
        p->engine_ready = false;
    }
}

void ds_player_play(ds_player *p) {
    if (!p || !p->sound_ready) return;
    if (ma_sound_is_playing(&p->sound)) {
        ma_sound_seek_to_pcm_frame(&p->sound, 0);
        return;
    }
    ma_sound_start(&p->sound);
}

void ds_player_pause(ds_player *p) {
    if (!p || !p->sound_ready) return;
    ma_sound_stop(&p->sound);
}

void ds_player_stop(ds_player *p) {
    if (!p || !p->sound_ready) return;
    ma_sound_stop(&p->sound);
    ma_sound_seek_to_pcm_frame(&p->sound, 0);
}

void ds_player_seek(ds_player *p, double seconds) {
    if (!p || !p->sound_ready) return;
    double dur = ds_player_duration(p);
    if (seconds <= 0.0) seconds = 0.0;
    if (seconds > dur) seconds = dur;
    ma_format fmt;
    ma_uint32 channels = 0, rate = 0;
    if (ma_sound_get_data_format(&p->sound, &fmt, &channels, &rate, NULL, 0) != MA_SUCCESS || rate == 0)
        return;
    ma_sound_seek_to_pcm_frame(&p->sound, (ma_uint64)(seconds * (double)rate));
}

bool ds_player_is_playing(const ds_player *p) {
    return p && p->sound_ready && ma_sound_is_playing(&p->sound);
}

bool ds_player_is_open(const ds_player *p) {
    return p && p->sound_ready;
}

double ds_player_position(const ds_player *p) {
    if (!p || !p->sound_ready) return 0.0;
    float v = 0.0f;
    if (ma_sound_get_cursor_in_seconds(&p->sound, &v) != MA_SUCCESS) return 0.0;
    return (double)v;
}

double ds_player_duration(const ds_player *p) {
    if (!p || !p->sound_ready) return 0.0;
    float v = 0.0f;
    if (ma_sound_get_length_in_seconds(&p->sound, &v) != MA_SUCCESS) return 0.0;
    return (double)v;
}
