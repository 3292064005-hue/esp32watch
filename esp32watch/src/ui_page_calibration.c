#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_calibration_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[24];

    (void)page;
    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Calibration");
    ui_core_draw_card(ox + 8, 14, 112, 22, snap.sensor.online ? "IMU READY" : "IMU OFFLINE");
    snprintf(line, sizeof(line), "%s  Q%u", snap.sensor.static_now ? "STATIC" : "MOVE", snap.sensor.quality);
    display_draw_text_centered_5x7(ox, 22, 128, line, true);
    display_draw_progress_bar(ox + 18, 39, 92, 8, snap.sensor.calibration_progress, false);
    snprintf(line, sizeof(line), "%u%%  BK %lu", snap.sensor.calibration_progress, (unsigned long)snap.sensor.retry_backoff_s);
    display_draw_text_centered_5x7(ox, 48, 128, line, true);
    ui_core_draw_footer_hint(ox, "OK Start  BK Back");
}

bool ui_page_calibration_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_LONG) && e->id == KEY_ID_OK) {
        ui_request_sensor_calibration();
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}
