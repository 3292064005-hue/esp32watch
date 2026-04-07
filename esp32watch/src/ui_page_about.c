#include "ui_internal.h"
#include "display.h"
#include "board_features.h"

void ui_page_about_render(PageId page, int16_t ox)
{
    (void)page;
    ui_core_draw_header(ox, "About");
    ui_core_draw_card(ox + 8, 14, 112, 36, "WATCH");
    display_draw_text_centered_5x7(ox, 22, 128, "ESP32 OLED WATCH UI", true);
    display_draw_text_centered_5x7(ox, 31, 128, "MODEL  SERVICES  DIAG", true);
#if APP_STORAGE_USE_FLASH
    display_draw_text_centered_5x7(ox, 40, 128, "FLASH TXN STORAGE", true);
#else
    display_draw_text_centered_5x7(ox, 40, 128, "RETENTION STORAGE", true);
#endif
    ui_core_draw_footer_hint(ox, "BK Back");
}

bool ui_page_about_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}
