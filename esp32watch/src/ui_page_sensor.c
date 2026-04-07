#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_sensor_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[24];

    (void)page;
    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Sensor");
    ui_core_draw_card(ox + 8, 14, 112, 22, snap.sensor.online ? "ONLINE" : "OFFLINE");
    snprintf(line, sizeof(line), "P%+d  R%+d", snap.sensor.pitch_deg, snap.sensor.roll_deg);
    display_draw_text_centered_5x7(ox, 22, 128, line, true);
    snprintf(line, sizeof(line), "Q%u  E%u", snap.sensor.quality, snap.sensor.error_code);
    ui_core_draw_kv_row(ox + 10, 39, 108, "STATE", line);
    snprintf(line, sizeof(line), "%u%%", snap.sensor.calibration_progress);
    ui_core_draw_kv_row(ox + 10, 47, 108, "CAL", line);
    ui_core_draw_footer_hint(ox, "OK Reinit  LONG Cal");
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
