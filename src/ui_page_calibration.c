#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_calibration_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[28];
    (void)page;

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Calibration");
    snprintf(line, sizeof(line), "imu %s", snap.sensor.online ? "online" : "offline");
    display_draw_text_centered_5x7(ox, 17, 128, line, true);
    snprintf(line, sizeof(line), "static %s  q%u", snap.sensor.static_now ? "yes" : "no", snap.sensor.quality);
    display_draw_text_centered_5x7(ox, 28, 128, line, true);
    snprintf(line, sizeof(line), "progress %u%%", snap.sensor.calibration_progress);
    display_draw_text_centered_5x7(ox, 39, 128, line, true);
    display_draw_progress_bar(ox + 18, 49, 92, 8, snap.sensor.calibration_progress, false);
    display_draw_text_centered_5x7(ox, 59, 128, "OK start/retry  BK back", true);
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
