#include "ui_internal.h"
#include "display.h"
#include "melody_service.h"
#include <stdio.h>

static uint8_t g_music_index;

static MelodyId ui_music_selected_song(void)
{
    return (MelodyId)(g_music_index % MELODY_COUNT);
}

static const char *ui_music_display_name(MelodyId song_id)
{
    return melody_song_ascii_name(song_id);
}

static const char *ui_music_selected_name(void)
{
    return ui_music_display_name(ui_music_selected_song());
}

void ui_page_music_render(PageId page, int16_t ox)
{
    MelodyState state;
    MelodyId current_song;
    char line[24];
    char status_line[24];
    char selected_line[24];

    (void)page;
    state = melody_get_state();
    current_song = melody_get_current_song();

    ui_core_draw_header(ox, "Melody");
    snprintf(status_line, sizeof(status_line), "%s", melody_is_available() ? melody_state_name(state) : "UNAVAILABLE");
    ui_core_draw_card(ox + 8, 14, 112, 14, status_line);
    snprintf(selected_line, sizeof(selected_line), "SEL %s", ui_music_selected_name());
    display_draw_text_centered_5x7(ox, 20, 128, selected_line, true);

    if (melody_is_available()) {
        snprintf(line, sizeof(line), "NOW %s",
                 melody_is_playing() ? ui_music_display_name(current_song) : melody_state_name(state));
    } else {
        snprintf(line, sizeof(line), "GPIO17 REQUIRED");
    }
    display_draw_text_centered_5x7(ox, 27, 128, line, false);

    for (uint8_t i = 0U; i < MELODY_COUNT; ++i) {
        bool selected = i == g_music_index;
        bool accent = melody_is_playing() && current_song == (MelodyId)i;

        ui_core_draw_list_item(ox, 36 + i * 9, 110, ui_music_display_name((MelodyId)i),
                               accent ? "PLAY" : "", selected, accent);
    }
    ui_core_draw_footer_hint(ox,
                             melody_is_available()
                                 ? (melody_is_playing() ? "PLAYING  OK Stop  BK Back" : "OK Play  BK Back")
                                 : "UNAVAILABLE  BK Back");
}

bool ui_page_music_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;

    if (e == NULL || e->type != KEY_EVENT_SHORT) {
        return false;
    }
    if (e->id == KEY_ID_UP && g_music_index > 0U) {
        g_music_index--;
        return true;
    }
    if (e->id == KEY_ID_DOWN && g_music_index + 1U < MELODY_COUNT) {
        g_music_index++;
        return true;
    }
    if (e->id == KEY_ID_OK && melody_is_available()) {
        if (melody_is_playing() && melody_get_current_song() == ui_music_selected_song()) {
            melody_stop();
        } else {
            melody_play(ui_music_selected_song());
        }
        return true;
    }
    if (e->id == KEY_ID_BACK) {
        melody_stop();
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}
