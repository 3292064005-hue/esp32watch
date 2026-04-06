#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

static const char *repeat_mask_label(uint8_t mask)
{
    if (mask == 0x7FU) return "Daily";
    if (mask == 0x3EU) return "Weekday";
    if (mask == 0x41U) return "Weekend";
    if (mask == 0U) return "Once";
    return "Custom";
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
        bool sel = i == domain_state.alarm_selected;
        char line[28];
        int16_t y = 16 + i * 12;
        if (sel) display_fill_round_rect(ox + 6, y - 1, 114, 10, true);
        snprintf(line, sizeof(line), "A%u %s %02u:%02u", i + 1U, a->enabled ? "ON " : "OFF", a->hour, a->minute);
        display_draw_text_5x7(ox + 12, y, line, !sel);
        snprintf(line, sizeof(line), "%s", repeat_mask_label(a->repeat_mask));
        display_draw_text_5x7(ox + 72, y + 7, line, !sel);
    }
    display_draw_text_centered_5x7(ox, 57, 128, "OK edit  LONG OK toggle", true);
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
