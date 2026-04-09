#include "melody_service.h"
#include "drv_buzzer.h"
#include "platform_api.h"
#include <string.h>

typedef struct {
    uint16_t pitch_hz;
    uint16_t duration_ms;
    uint16_t rest_after_ms;
} MelodyNote;

typedef struct {
    const char *name;
    const MelodyNote *notes;
    uint16_t note_count;
} MelodySong;

typedef struct {
    bool initialized;
    MelodyState state;
    MelodyId current_song;
    uint16_t note_index;
    bool in_rest;
    uint32_t phase_until_ms;
} MelodyRuntime;

enum {
    NOTE_C3 = 131, NOTE_CS3 = 139, NOTE_D3 = 147, NOTE_DS3 = 156, NOTE_E3 = 165,
    NOTE_F3 = 175, NOTE_FS3 = 185, NOTE_G3 = 196, NOTE_GS3 = 208, NOTE_A3 = 220,
    NOTE_AS3 = 233, NOTE_B3 = 247, NOTE_C4 = 262, NOTE_CS4 = 277, NOTE_D4 = 294,
    NOTE_DS4 = 311, NOTE_E4 = 330, NOTE_F4 = 349, NOTE_FS4 = 370, NOTE_G4 = 392,
    NOTE_GS4 = 415, NOTE_A4 = 440, NOTE_AS4 = 466, NOTE_B4 = 494, NOTE_C5 = 523,
    NOTE_CS5 = 554, NOTE_D5 = 587, NOTE_DS5 = 622, NOTE_E5 = 659, NOTE_FS5 = 740, NOTE_G5 = 784,
    NOTE_A5 = 880
};

enum {
    DUR_PICKUP = 180,
    DUR_SHORT = 240,
    DUR_MEDIUM = 320,
    DUR_LONG = 480,
    DUR_HOLD = 640,
    REST_GAP = 35,
    REST_BAR = 110
};

enum {
    MELODY_TEMPO_SCALE = 2
};

/*
 * Third-party references used for the short melody fragments:
 * - 晴天（果木音乐版简谱）https://www.qupu123.com/Public/Uploads/2019/09/02/4234625d6cb29d54ef3.jpg
 * - 七里香（中国曲谱网简谱）https://www.qupu123.com/Public/Uploads/3/b/50604ba0f34ec.jpg
 * - 青花瓷（中国曲谱网简谱）https://www.qupu123.com/Public/Uploads/2017/07/28/137823597a94c2b6450.png
 */
static const MelodyNote g_qing_tian_notes[] = {
    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR},
    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR},

    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_SHORT,  REST_GAP},
    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_MEDIUM, REST_BAR},

    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_SHORT,  REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_MEDIUM, REST_BAR},

    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_SHORT,  REST_GAP},
    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_MEDIUM, REST_BAR},

    {0,        DUR_SHORT,  REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {0,        DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_G5,  DUR_PICKUP, REST_GAP},
    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_G5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {0,        DUR_MEDIUM, REST_BAR},

    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR},
    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR},
    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR},
    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR},

    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_G5,  DUR_SHORT,  REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_MEDIUM, REST_BAR},

    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_DS5, DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {0,        DUR_SHORT,  REST_GAP},
    {NOTE_D5,  DUR_MEDIUM, REST_BAR},

    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_CS5, DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_CS5, DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_MEDIUM, REST_BAR},

    {0,        DUR_SHORT,  REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_CS5, DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_MEDIUM, REST_BAR},

    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_G4,  DUR_PICKUP, REST_GAP}, {NOTE_G4,  DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_MEDIUM, REST_BAR},

    {NOTE_FS5, DUR_LONG,   REST_GAP}, {NOTE_FS5, DUR_LONG,   REST_BAR},
    {0,        DUR_HOLD,   REST_BAR}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_CS5, DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_MEDIUM, REST_BAR},

    {NOTE_B4,  DUR_PICKUP, REST_GAP}, {NOTE_CS5, DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_G5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_MEDIUM, REST_BAR},

    {NOTE_D5,  DUR_MEDIUM, REST_GAP}, {0,        DUR_SHORT,  REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {0,        DUR_MEDIUM, REST_BAR},

    {NOTE_B4,  DUR_PICKUP, REST_GAP}, {NOTE_CS5, DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_G5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_MEDIUM, REST_BAR},

    {NOTE_E5,  DUR_LONG,   REST_GAP}, {NOTE_E5,  DUR_LONG,   REST_BAR},
    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR},

    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_G5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {0,        DUR_SHORT,  REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_GS4, DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_MEDIUM, REST_BAR},

    {NOTE_CS5, DUR_PICKUP, REST_GAP}, {NOTE_CS5, DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {0,        DUR_SHORT,  REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_B4,  DUR_MEDIUM, REST_BAR},

    {0,        DUR_SHORT,  REST_GAP}, {NOTE_B4,  DUR_PICKUP, REST_GAP},
    {NOTE_A4,  DUR_PICKUP, REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_G4,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_MEDIUM, REST_BAR},

    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_G5,  DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_MEDIUM, REST_BAR},

    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_GS4, DUR_PICKUP, REST_GAP},
    {NOTE_AS4, DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_AS4, DUR_PICKUP, REST_GAP}, {NOTE_CS5, DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_MEDIUM, REST_BAR},

    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_GS4, DUR_PICKUP, REST_GAP},
    {NOTE_AS4, DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {0,        DUR_SHORT,  REST_GAP}, {NOTE_A4,  DUR_PICKUP, REST_GAP},
    {NOTE_AS4, DUR_PICKUP, REST_GAP}, {NOTE_CS5, DUR_PICKUP, REST_GAP},
    {NOTE_E5,  DUR_MEDIUM, REST_BAR},

    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_CS5, DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_PICKUP, REST_GAP}, {NOTE_D5,  DUR_PICKUP, REST_GAP},
    {NOTE_D5,  DUR_LONG,   REST_GAP}, {0,        DUR_HOLD,   REST_BAR},

    {NOTE_E5,  DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_PICKUP, REST_GAP},
    {NOTE_G5,  DUR_PICKUP, REST_GAP}, {NOTE_E5,  DUR_PICKUP, REST_GAP},
    {NOTE_FS5, DUR_PICKUP, REST_GAP}, {NOTE_FS5, DUR_MEDIUM, REST_BAR},

    {NOTE_D5,  DUR_LONG,   REST_GAP}, {NOTE_D5,  DUR_LONG,   REST_BAR},
    {0,        DUR_HOLD,   REST_BAR}, {0,        DUR_HOLD,   REST_BAR}
};

static const MelodyNote g_qi_li_xiang_notes[] = {
    {NOTE_D4, DUR_PICKUP, REST_GAP}, {NOTE_E4, DUR_PICKUP, REST_GAP},
    {NOTE_G4, DUR_PICKUP, REST_GAP}, {NOTE_A4, DUR_PICKUP, REST_BAR},
    {NOTE_C5, DUR_SHORT, REST_GAP}, {NOTE_C5, DUR_SHORT, REST_GAP},
    {NOTE_E5, DUR_SHORT, REST_GAP}, {NOTE_D5, DUR_SHORT, REST_GAP},
    {NOTE_C5, DUR_SHORT, REST_GAP}, {NOTE_C5, DUR_SHORT, REST_GAP},
    {NOTE_A4, DUR_MEDIUM, REST_BAR},
    {NOTE_G4, DUR_SHORT, REST_GAP}, {NOTE_A4, DUR_SHORT, REST_GAP},
    {NOTE_C5, DUR_SHORT, REST_GAP}, {NOTE_D5, DUR_SHORT, REST_GAP},
    {NOTE_E5, DUR_MEDIUM, REST_BAR},
    {NOTE_D4, DUR_PICKUP, REST_GAP}, {NOTE_E4, DUR_PICKUP, REST_GAP},
    {NOTE_G4, DUR_PICKUP, REST_GAP}, {NOTE_A4, DUR_PICKUP, REST_BAR},
    {NOTE_C5, DUR_SHORT, REST_GAP}, {NOTE_C5, DUR_SHORT, REST_GAP},
    {NOTE_E5, DUR_SHORT, REST_GAP}, {NOTE_D5, DUR_SHORT, REST_GAP},
    {NOTE_C5, DUR_SHORT, REST_GAP}, {NOTE_C5, DUR_SHORT, REST_GAP},
    {NOTE_A4, DUR_MEDIUM, REST_BAR},
    {NOTE_D4, DUR_LONG, REST_GAP}, {NOTE_G4, DUR_SHORT, REST_GAP},
    {NOTE_C5, DUR_SHORT, REST_GAP}, {NOTE_B4, DUR_SHORT, REST_GAP},
    {NOTE_C5, DUR_SHORT, REST_GAP}, {NOTE_B4, DUR_SHORT, REST_GAP},
    {NOTE_A4, DUR_SHORT, REST_GAP}, {NOTE_G4, DUR_SHORT, REST_GAP},
    {NOTE_G4, DUR_LONG, REST_BAR}
};

static const MelodyNote g_qing_hua_ci_notes[] = {
    {NOTE_D4, DUR_PICKUP, REST_GAP}, {NOTE_C4, DUR_PICKUP, REST_GAP},
    {NOTE_A3, DUR_SHORT, REST_BAR},
    {NOTE_C4, DUR_SHORT, REST_GAP}, {NOTE_C4, DUR_SHORT, REST_GAP},
    {NOTE_A3, DUR_SHORT, REST_GAP}, {NOTE_C4, DUR_SHORT, REST_GAP},
    {NOTE_C4, DUR_SHORT, REST_GAP}, {NOTE_A3, DUR_SHORT, REST_GAP},
    {NOTE_C4, DUR_SHORT, REST_GAP}, {NOTE_A3, DUR_SHORT, REST_GAP},
    {NOTE_G3, DUR_MEDIUM, REST_BAR},
    {NOTE_D4, DUR_PICKUP, REST_GAP}, {NOTE_C4, DUR_PICKUP, REST_GAP},
    {NOTE_A3, DUR_SHORT, REST_BAR},
    {NOTE_C4, DUR_SHORT, REST_GAP}, {NOTE_C4, DUR_SHORT, REST_GAP},
    {NOTE_A3, DUR_SHORT, REST_GAP}, {NOTE_C4, DUR_SHORT, REST_GAP},
    {NOTE_C4, DUR_SHORT, REST_GAP}, {NOTE_E4, DUR_SHORT, REST_GAP},
    {NOTE_D4, DUR_SHORT, REST_GAP}, {NOTE_C4, DUR_SHORT, REST_GAP},
    {NOTE_C4, DUR_SHORT, REST_BAR},
    {NOTE_G3, DUR_PICKUP, REST_GAP}, {NOTE_A3, DUR_PICKUP, REST_GAP},
    {NOTE_E4, DUR_SHORT, REST_GAP}, {NOTE_D4, DUR_SHORT, REST_GAP},
    {NOTE_E4, DUR_SHORT, REST_GAP}, {NOTE_D4, DUR_SHORT, REST_GAP},
    {NOTE_C4, DUR_SHORT, REST_GAP}, {NOTE_A3, DUR_SHORT, REST_GAP},
    {NOTE_A3, DUR_LONG, REST_BAR}
};

static const MelodySong g_songs[MELODY_COUNT] = {
    {"\xE6\x99\xB4\xE5\xA4\xA9", g_qing_tian_notes, (uint16_t)(sizeof(g_qing_tian_notes) / sizeof(g_qing_tian_notes[0]))},
    {"\xE4\xB8\x83\xE9\x87\x8C\xE9\xA6\x99", g_qi_li_xiang_notes, (uint16_t)(sizeof(g_qi_li_xiang_notes) / sizeof(g_qi_li_xiang_notes[0]))},
    {"\xE9\x9D\x92\xE8\x8A\xB1\xE7\x93\xB7", g_qing_hua_ci_notes, (uint16_t)(sizeof(g_qing_hua_ci_notes) / sizeof(g_qing_hua_ci_notes[0]))}
};

static MelodyRuntime g_melody;

static uint32_t melody_scaled_ms(uint16_t duration_ms)
{
    return (uint32_t)duration_ms * MELODY_TEMPO_SCALE;
}

static void melody_start_note(const MelodyNote *note, uint32_t now_ms)
{
    if (note == NULL) {
        return;
    }

    drv_buzzer_play_tone(note->pitch_hz);
    g_melody.in_rest = false;
    g_melody.phase_until_ms = now_ms + melody_scaled_ms(note->duration_ms);
}

void melody_init(void)
{
    memset(&g_melody, 0, sizeof(g_melody));
    g_melody.initialized = true;
    g_melody.state = MELODY_STATE_IDLE;
    drv_buzzer_init();
}

void melody_tick(uint32_t now_ms)
{
    const MelodySong *song;
    const MelodyNote *note;

    if (!g_melody.initialized) {
        melody_init();
        g_melody.initialized = true;
    }
    if (g_melody.state != MELODY_STATE_PLAYING || g_melody.current_song >= MELODY_COUNT) {
        return;
    }
    if (now_ms < g_melody.phase_until_ms) {
        return;
    }

    song = &g_songs[g_melody.current_song];
    if (song->note_count == 0U) {
        drv_buzzer_stop();
        g_melody.state = MELODY_STATE_DONE;
        return;
    }

    note = &song->notes[g_melody.note_index];
    if (!g_melody.in_rest && note->rest_after_ms > 0U) {
        drv_buzzer_stop();
        g_melody.in_rest = true;
        g_melody.phase_until_ms = now_ms + melody_scaled_ms(note->rest_after_ms);
        return;
    }

    g_melody.note_index = (uint16_t)(g_melody.note_index + 1U);
    if (g_melody.note_index >= song->note_count) {
        drv_buzzer_set_playback_mode(false);
        drv_buzzer_stop();
        g_melody.state = MELODY_STATE_DONE;
        g_melody.in_rest = false;
        g_melody.phase_until_ms = 0U;
        return;
    }

    melody_start_note(&song->notes[g_melody.note_index], now_ms);
}

void melody_play(MelodyId song_id)
{
    uint32_t now_ms;

    if (song_id >= MELODY_COUNT || !drv_buzzer_is_available()) {
        return;
    }

    if (!g_melody.initialized) {
        melody_init();
    }
    melody_stop();
    g_melody.current_song = song_id;
    g_melody.note_index = 0U;
    g_melody.state = MELODY_STATE_PLAYING;
    drv_buzzer_set_playback_mode(true);
    now_ms = platform_time_now_ms();
    melody_start_note(&g_songs[song_id].notes[0], now_ms);
}

void melody_stop(void)
{
    drv_buzzer_set_playback_mode(false);
    drv_buzzer_stop();
    g_melody.state = MELODY_STATE_IDLE;
    g_melody.note_index = 0U;
    g_melody.in_rest = false;
    g_melody.phase_until_ms = 0U;
}

bool melody_is_available(void)
{
    return drv_buzzer_is_available();
}

bool melody_is_playing(void)
{
    return g_melody.state == MELODY_STATE_PLAYING;
}

MelodyState melody_get_state(void)
{
    return g_melody.state;
}

MelodyId melody_get_current_song(void)
{
    return g_melody.current_song;
}

const char *melody_song_name(MelodyId song_id)
{
    return song_id < MELODY_COUNT ? g_songs[song_id].name : "Song";
}

const char *melody_song_ascii_name(MelodyId song_id)
{
    switch (song_id) {
        case MELODY_QING_TIAN: return "QingTian";
        case MELODY_QI_LI_XIANG: return "QiLiXiang";
        case MELODY_QING_HUA_CI: return "QingHuaCi";
        default: return "Song";
    }
}

const char *melody_state_name(MelodyState state)
{
    switch (state) {
        case MELODY_STATE_PLAYING: return "PLAYING";
        case MELODY_STATE_DONE: return "DONE";
        default: return "IDLE";
    }
}
