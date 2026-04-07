#include "ui_internal.h"
#include "display.h"

void ui_page_inputtest_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;

    (void)page;
    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Input Test");
    ui_core_draw_card(ox + 8, 14, 112, 32, "KEYS");
    ui_core_draw_kv_row(ox + 10, 20, 108, "UP", snap.key_up_down ? "DOWN" : "UP");
    ui_core_draw_kv_row(ox + 10, 28, 108, "DOWN", snap.key_down_down ? "DOWN" : "UP");
    ui_core_draw_kv_row(ox + 10, 36, 108, "OK", snap.key_ok_down ? "DOWN" : "UP");
    ui_core_draw_kv_row(ox + 10, 44, 108, "BACK", snap.key_back_down ? "DOWN" : "UP");
    ui_core_draw_footer_hint(ox, "BK Back");
}

bool ui_page_inputtest_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}
