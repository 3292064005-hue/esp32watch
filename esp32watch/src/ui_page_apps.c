#include "ui_internal.h"
#include "ui_app_catalog.h"
#include "display.h"

void ui_page_apps_render(PageId page, int16_t ox)
{
    uint8_t total = ui_app_catalog_count();
    uint8_t page_start = (ui_runtime_get_app_index() / 4U) * 4U;

    (void)page;
    ui_core_draw_header(ox, "Apps");
    for (uint8_t i = 0U; i < 4U; ++i) {
        uint8_t idx = page_start + i;
        UiAppDescriptor app = {};
        char value[16] = "";

        if (!ui_app_catalog_get(idx, &app)) {
            break;
        }
        if (app.compose_status != NULL) {
            app.compose_status(value, sizeof(value));
        }
        ui_core_draw_list_item(ox, 14 + i * 10, 110, app.name, value, idx == ui_runtime_get_app_index(), app.accent);
    }
    ui_core_draw_scrollbar(ox + 121, 14, 40, total, ui_runtime_get_app_index());
    ui_core_draw_footer_hint(ox, "OK Open  BK Home");
}

bool ui_page_apps_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    uint8_t total = ui_app_catalog_count();
    UiAppDescriptor app = {};

    (void)page;
    if (e->type != KEY_EVENT_SHORT) return false;
    if (e->id == KEY_ID_UP && ui_runtime_get_app_index() > 0U) ui_runtime_set_app_index((uint8_t)(ui_runtime_get_app_index() - 1U));
    else if (e->id == KEY_ID_DOWN && ui_runtime_get_app_index() + 1U < total) ui_runtime_set_app_index((uint8_t)(ui_runtime_get_app_index() + 1U));
    else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_WATCHFACE, -1, now_ms);
    else if (e->id == KEY_ID_OK && ui_app_catalog_get(ui_runtime_get_app_index(), &app)) ui_core_go(app.page, 1, now_ms);
    else return false;
    return true;
}
