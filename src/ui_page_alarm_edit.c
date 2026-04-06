#include "ui_internal.h"
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

static uint8_t cycle_repeat_mask(uint8_t current, int8_t dir)
{
    static const uint8_t presets[] = {0x7FU, 0x3EU, 0x41U, 0x00U};
    int idx = 0;
    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); ++i) {
        if (presets[i] == current) { idx = (int)i; break; }
    }
    idx += dir;
    if (idx < 0) idx = (int)(sizeof(presets) / sizeof(presets[0])) - 1;
    if (idx >= (int)(sizeof(presets) / sizeof(presets[0]))) idx = 0;
    return presets[idx];
}

static void adjust_alarm(int8_t delta)
{
    ModelDomainState domain_state;

    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }
    if (ui_runtime_get_alarm_field() == 1U) {
        int v = (int)domain_state.selected_alarm.hour + delta;
        if (v < 0) v = 23;
        if (v > 23) v = 0;
        ui_request_set_alarm_time_at(domain_state.alarm_selected, (uint8_t)v, domain_state.selected_alarm.minute);
    } else if (ui_runtime_get_alarm_field() == 2U) {
        int v = (int)domain_state.selected_alarm.minute + delta;
        if (v < 0) v = 59;
        if (v > 59) v = 0;
        ui_request_set_alarm_time_at(domain_state.alarm_selected, domain_state.selected_alarm.hour, (uint8_t)v);
    } else if (ui_runtime_get_alarm_field() == 3U) {
        ui_request_set_alarm_repeat_mask_at(domain_state.alarm_selected, cycle_repeat_mask(domain_state.selected_alarm.repeat_mask, delta > 0 ? 1 : -1));
    }
}

void ui_page_alarm_edit_render(PageId page, int16_t ox)
{
    ModelDomainState domain_state;
    char line[24];

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }
    ui_core_draw_header(ox, "Alarm Edit");
    snprintf(line, sizeof(line), "A%u  %02u:%02u", domain_state.alarm_selected + 1U, domain_state.selected_alarm.hour, domain_state.selected_alarm.minute);
    display_draw_text_centered_5x7(ox, 18, 128, line, true);
    for (uint8_t i = 0; i < 5U; ++i) {
        bool sel = i == ui_runtime_get_alarm_field();
        int16_t y = 30 + i * 8;
        if (sel) display_fill_round_rect(ox + 8, y - 1, 112, 8, true);
        if (i == 0U) snprintf(line, sizeof(line), "%c enable  %s", sel && ui_runtime_is_alarm_editing() ? '*' : ' ', domain_state.selected_alarm.enabled ? "ON" : "OFF");
        else if (i == 1U) snprintf(line, sizeof(line), "%c hour    %02u", sel && ui_runtime_is_alarm_editing() ? '*' : ' ', domain_state.selected_alarm.hour);
        else if (i == 2U) snprintf(line, sizeof(line), "%c minute  %02u", sel && ui_runtime_is_alarm_editing() ? '*' : ' ', domain_state.selected_alarm.minute);
        else if (i == 3U) snprintf(line, sizeof(line), "%c repeat  %s", sel && ui_runtime_is_alarm_editing() ? '*' : ' ', repeat_mask_label(domain_state.selected_alarm.repeat_mask));
        else snprintf(line, sizeof(line), "  done");
        display_draw_text_5x7(ox + 12, y, line, !sel);
    }
}

bool ui_page_alarm_edit_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    ModelDomainState domain_state;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return false;
    }
    if (ui_runtime_is_alarm_editing()) {
        if (e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT || e->type == KEY_EVENT_LONG) {
            if (e->id == KEY_ID_UP) adjust_alarm(1);
            else if (e->id == KEY_ID_DOWN) adjust_alarm(-1);
            else if (e->id == KEY_ID_OK || e->id == KEY_ID_BACK) ui_runtime_set_alarm_editing(false);
            else return false;
            return true;
        }
    } else if (e->type == KEY_EVENT_SHORT) {
        if (e->id == KEY_ID_UP && ui_runtime_get_alarm_field() > 0U) ui_runtime_set_alarm_field((uint8_t)(ui_runtime_get_alarm_field() - 1U));
        else if (e->id == KEY_ID_DOWN && ui_runtime_get_alarm_field() < 3U) ui_runtime_set_alarm_field((uint8_t)(ui_runtime_get_alarm_field() + 1U));
        else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_ALARM, -1, now_ms);
        else if (e->id == KEY_ID_OK) {
            if (ui_runtime_get_alarm_field() == 0U) ui_request_set_alarm_enabled_at(domain_state.alarm_selected, !domain_state.selected_alarm.enabled);
            else if (ui_runtime_get_alarm_field() == 1U || ui_runtime_get_alarm_field() == 2U || ui_runtime_get_alarm_field() == 3U) ui_runtime_set_alarm_editing(true);
            else ui_core_go(PAGE_ALARM, -1, now_ms);
        } else return false;
        return true;
    }
    return false;
}
