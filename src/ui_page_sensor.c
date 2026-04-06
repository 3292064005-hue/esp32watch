#include "ui_internal.h"
#include "display.h"
#include "services/sensor_service.h"
#include <stdio.h>

void ui_page_sensor_render(PageId page, int16_t ox)
{
    const SensorSnapshot *snap;
    char line[24];
    (void)page;

    snap = sensor_service_get_snapshot();
    display_draw_text_centered_5x7(ox, 3, 128, "ANGLE", true);
    snprintf(line, sizeof(line), "X:%4d", (snap != NULL) ? snap->roll_deg : 0);
    display_draw_text_centered_5x7(ox, 18, 128, line, true);
    snprintf(line, sizeof(line), "Y:%4d", (snap != NULL) ? snap->pitch_deg : 0);
    display_draw_text_centered_5x7(ox, 32, 128, line, true);
    snprintf(line, sizeof(line), "Z:%4d", (snap != NULL) ? snap->yaw_deg : 0);
    display_draw_text_centered_5x7(ox, 46, 128, line, true);
}

bool ui_page_sensor_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_OK) {
        ui_request_sensor_reinit();
        return true;
    }
    if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_OK) {
        ui_request_sensor_calibration();
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}
