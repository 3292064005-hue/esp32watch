#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

static const char *repeat_mask_label(uint8_t mask)
{
    if (mask == 0x7FU) return "DLY";
    if (mask == 0x3EU) return "WKD";
    if (mask == 0x41U) return "WND";
    if (mask == 0U) return "ONE";
    return "CST";
}

void ui_page_alarm_render(PageId page, int16_t ox)
{
    ModelDomainState domain_state;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }

    ui_core_draw_header(ox, "Alarms");
    for (uint8_t i = 0; i < APP_MAX_ALARMS; ++i) {
        const AlarmState *a = &domain_state.alarms[i];
        char label[20];
        char value[20];

        snprintf(label, sizeof(label), "Alarm %u %s", i + 1U, repeat_mask_label(a->repeat_mask));
        snprintf(value, sizeof(value), "%02u:%02u %s", a->hour, a->minute, a->enabled ? "ON" : "OFF");
        ui_core_draw_list_item(ox, 14 + i * 12, 110, label, value, i == domain_state.alarm_selected, false);
    }
    ui_core_draw_footer_hint(ox, "OK Edit  LONG Toggle");
}

bool ui_page_alarm_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    ModelDomainState domain_state;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return false;
    }
    if (e->type == KEY_EVENT_SHORT) {
        if (e->id == KEY_ID_UP) ui_request_select_alarm_offset(-1);
        else if (e->id == KEY_ID_DOWN) ui_request_select_alarm_offset(1);
        else if (e->id == KEY_ID_OK) { ui_runtime_set_alarm_field(0U); ui_runtime_set_alarm_editing(false); ui_core_go(PAGE_ALARM_EDIT, 1, now_ms); }
        else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_APPS, -1, now_ms);
        else return false;
        return true;
    }
    if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_OK) {
        ui_request_set_alarm_enabled_at(domain_state.alarm_selected, !domain_state.selected_alarm.enabled);
        return true;
    }
    return false;
}
