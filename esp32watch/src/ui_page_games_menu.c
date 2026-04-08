#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

static void game_menu_label(uint8_t index, char *out, size_t out_len)
{
    uint16_t hi = model_get_game_high_score(ui_game_id_from_index(index));

    if (out == NULL || out_len == 0U) {
        return;
    }
    if (hi == 0U) {
        snprintf(out, out_len, "NEW");
    } else {
        snprintf(out, out_len, "HI %u", (unsigned)hi);
    }
}

void ui_page_games_menu_render(PageId page, int16_t ox)
{
    uint8_t total = ui_game_count();
    uint8_t index = ui_runtime_get_game_index();
    uint8_t page_start;

    (void)page;
    if (index >= total) {
        index = (uint8_t)(total - 1U);
        ui_runtime_set_game_index(index);
    }

    page_start = (index / 4U) * 4U;
    ui_core_draw_header(ox, "Games");
    for (uint8_t i = 0U; i < 4U; ++i) {
        uint8_t idx = (uint8_t)(page_start + i);
        char hi_label[12];

        if (idx >= total) {
            break;
        }
        game_menu_label(idx, hi_label, sizeof(hi_label));
        ui_core_draw_list_item(ox, 14 + i * 10, 110,
                               ui_game_name(ui_game_id_from_index(idx)),
                               hi_label,
                               idx == index,
                               false);
    }
    ui_core_draw_scrollbar(ox + 121, 14, 40, total, index);
    ui_core_draw_footer_hint(ox, "OK Detail  BK Apps");
}

bool ui_page_games_menu_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    uint8_t total = ui_game_count();
    uint8_t index = ui_runtime_get_game_index();

    (void)page;
    if (index >= total) {
        index = (uint8_t)(total - 1U);
        ui_runtime_set_game_index(index);
    }

    if (e->type != KEY_EVENT_SHORT) {
        return false;
    }
    if (e->id == KEY_ID_UP && index > 0U) {
        ui_runtime_set_game_index((uint8_t)(index - 1U));
    } else if (e->id == KEY_ID_DOWN && index + 1U < total) {
        ui_runtime_set_game_index((uint8_t)(index + 1U));
    } else if (e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
    } else if (e->id == KEY_ID_OK) {
        ui_runtime_set_game_detail_index(index);
        ui_core_go(PAGE_GAME_DETAIL, 1, now_ms);
    } else {
        return false;
    }
    return true;
}
