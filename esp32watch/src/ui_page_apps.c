#include "ui_internal.h"
#include "display.h"

static const char * const app_names[] = {
    "Alarms", "Stopwatch", "Timer", "Activity", "Sensor",
    "Liquid", "Settings", "Diag", "Calib", "Input", "Storage", "About"
};
static const PageId app_pages[] = {
    PAGE_ALARM, PAGE_STOPWATCH, PAGE_TIMER, PAGE_ACTIVITY, PAGE_SENSOR,
    PAGE_LIQUID, PAGE_SETTINGS, PAGE_DIAG, PAGE_CALIBRATION, PAGE_INPUTTEST, PAGE_STORAGE, PAGE_ABOUT
};

void ui_page_apps_render(PageId page, int16_t ox)
{
    uint8_t total = (uint8_t)(sizeof(app_names) / sizeof(app_names[0]));
    uint8_t page_start = (ui_runtime_get_app_index() / 4U) * 4U;

    (void)page;
    ui_core_draw_header(ox, "Apps");
    for (uint8_t i = 0U; i < 4U; ++i) {
        uint8_t idx = page_start + i;

        if (idx >= total) break;
        ui_core_draw_list_item(ox, 14 + i * 10, 110, app_names[idx], "", idx == ui_runtime_get_app_index(), false);
    }
    ui_core_draw_scrollbar(ox + 121, 14, 40, total, ui_runtime_get_app_index());
    ui_core_draw_footer_hint(ox, "OK Open  BK Watch");
}

bool ui_page_apps_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    uint8_t total = (uint8_t)(sizeof(app_names) / sizeof(app_names[0]));

    (void)page;
    if (e->type != KEY_EVENT_SHORT) return false;
    if (e->id == KEY_ID_UP && ui_runtime_get_app_index() > 0U) ui_runtime_set_app_index((uint8_t)(ui_runtime_get_app_index() - 1U));
    else if (e->id == KEY_ID_DOWN && ui_runtime_get_app_index() + 1U < total) ui_runtime_set_app_index((uint8_t)(ui_runtime_get_app_index() + 1U));
    else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_WATCHFACE, -1, now_ms);
    else if (e->id == KEY_ID_OK) ui_core_go(app_pages[ui_runtime_get_app_index()], 1, now_ms);
    else return false;
    return true;
}
