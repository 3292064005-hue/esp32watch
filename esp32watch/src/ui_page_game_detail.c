#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

static uint8_t ui_game_detail_index_clamped(void)
{
    uint8_t index = ui_runtime_get_game_detail_index();

    if (index >= ui_game_count()) {
        index = 0U;
        ui_runtime_set_game_detail_index(index);
    }
    return index;
}

void ui_page_game_detail_render(PageId page, int16_t ox)
{
    uint8_t index = ui_game_detail_index_clamped();
    GameId game_id = ui_game_id_from_index(index);
    char hi_text[16];

    (void)page;
    ui_core_draw_header(ox, ui_game_name(game_id));
    ui_core_draw_card(ox + 8, 14, 112, 16, ui_game_detail_tag(game_id));
    if (model_get_game_high_score(game_id) == 0U) snprintf(hi_text, sizeof(hi_text), "NEW"); else snprintf(hi_text, sizeof(hi_text), "HI %u", (unsigned)model_get_game_high_score(game_id));
    display_draw_text_5x7(ox + 16, 21, hi_text, true);
    display_draw_text_right_5x7(ox + 112, 21, "ARCADE", true);

    ui_core_draw_card(ox + 8, 32, 112, 20, "Info");
    display_draw_text_5x7(ox + 14, 41, ui_game_detail_desc(game_id), true);
    display_draw_text_5x7(ox + 14, 51, ui_game_detail_controls_a(game_id), true);
    display_draw_text_right_5x7(ox + 118, 51, ui_game_detail_controls_b(game_id), true);
    ui_core_draw_footer_hint(ox, "OK Start  BK Games");
}

bool ui_page_game_detail_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    uint8_t index = ui_game_detail_index_clamped();

    (void)page;
    if (e->type != KEY_EVENT_SHORT) {
        return false;
    }
    if (e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_GAMES, -1, now_ms);
        return true;
    }
    if (e->id == KEY_ID_OK) {
        PageId next = ui_game_page_from_index(index);
        ui_runtime_set_game_index(index);
        ui_core_go(next, 1, now_ms);
        return true;
    }
    return false;
}

