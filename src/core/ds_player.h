// ds_player.h - audio playback for the generated clips (miniaudio, vendored).
#ifndef DS_PLAYER_H
#define DS_PLAYER_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_player ds_player;

ds_player *ds_player_create(void);
void ds_player_destroy(ds_player *p);

bool ds_player_open(ds_player *p, const char *path, char *err, size_t cap);
void ds_player_close(ds_player *p);

void ds_player_play(ds_player *p);
void ds_player_pause(ds_player *p);
void ds_player_stop(ds_player *p);
void ds_player_seek(ds_player *p, double seconds);

bool ds_player_is_playing(const ds_player *p);
bool ds_player_is_open(const ds_player *p);
double ds_player_position(const ds_player *p);
double ds_player_duration(const ds_player *p);

#ifdef __cplusplus
}
#endif

#endif
