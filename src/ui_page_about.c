#include "ui_internal.h"
#include "display.h"
#include "board_features.h"

void ui_page_about_render(PageId page, int16_t ox)
{
    (void)page;
    ui_core_draw_header(ox, "About");
    display_draw_text_5x7(ox + 10, 14, "F103 watch terminal", true);
    display_draw_text_5x7(ox + 10, 24, "service + model split", true);
    display_draw_text_5x7(ox + 10, 34, "diag log + safe refresh", true);
#if APP_STORAGE_USE_FLASH
    display_draw_text_5x7(ox + 10, 44, "flash txn storage path", true);
#else
    display_draw_text_5x7(ox + 10, 44, "bkp retention storage", true);
#endif
    display_draw_text_centered_5x7(ox, 58, 128, "BACK return", true);
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
