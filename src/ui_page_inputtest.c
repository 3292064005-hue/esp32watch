#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_inputtest_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[20];
    (void)page;

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Input Test");
    snprintf(line, sizeof(line), "UP   %s", snap.key_up_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 16, line, true);
    snprintf(line, sizeof(line), "DOWN %s", snap.key_down_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 27, line, true);
    snprintf(line, sizeof(line), "OK   %s", snap.key_ok_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 38, line, true);
    snprintf(line, sizeof(line), "BACK %s", snap.key_back_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 49, line, true);
    display_draw_text_centered_5x7(ox, 60, 128, "press keys  BK back", true);
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
