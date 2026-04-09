#ifndef MELODY_SERVICE_H
#define MELODY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MELODY_QING_TIAN = 0,
    MELODY_QI_LI_XIANG,
    MELODY_QING_HUA_CI,
    MELODY_COUNT
} MelodyId;

typedef enum {
    MELODY_STATE_IDLE = 0,
    MELODY_STATE_PLAYING,
    MELODY_STATE_DONE
} MelodyState;

void melody_init(void);
void melody_tick(uint32_t now_ms);
void melody_play(MelodyId song_id);
void melody_stop(void);
bool melody_is_available(void);
bool melody_is_playing(void);
MelodyState melody_get_state(void);
MelodyId melody_get_current_song(void);
const char *melody_song_name(MelodyId song_id);
const char *melody_song_ascii_name(MelodyId song_id);
const char *melody_state_name(MelodyState state);

#ifdef __cplusplus
}
#endif

#endif
