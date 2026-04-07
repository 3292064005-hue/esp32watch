#include "ui_internal.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

static const char *repeat_mask_label(uint8_t mask)
{
    if (mask == 0x7FU) return "DAILY";
    if (mask == 0x3EU) return "WEEKDAY";
    if (mask == 0x41U) return "WEEKEND";
    if (mask == 0U) return "ONCE";
    return "CUSTOM";
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
    const char *fields[] = {"EN", "HR", "MIN", "RPT", "OK"};

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }

    ui_core_draw_header(ox, "Alarm Edit");
    snprintf(line, sizeof(line), "ALARM %u", domain_state.alarm_selected + 1U);
    ui_core_draw_card(ox + 8, 14, 112, 23, line);
    snprintf(line, sizeof(line), "%02u:%02u", domain_state.selected_alarm.hour, domain_state.selected_alarm.minute);
    display_draw_text_centered_5x7(ox, 22, 128, line, true);
    snprintf(line, sizeof(line), "%s  %s",
             domain_state.selected_alarm.enabled ? "ON" : "OFF",
             repeat_mask_label(domain_state.selected_alarm.repeat_mask));
    display_draw_text_centered_5x7(ox, 30, 128, line, true);

    for (uint8_t i = 0; i < 5U; ++i) {
        int16_t bx = ox + 6 + (int16_t)i * 23;
        bool sel = i == ui_runtime_get_alarm_field();

        if (sel) {
            display_fill_round_rect(bx, 42, 20, 10, true);
            if (ui_runtime_is_alarm_editing()) {
                display_fill_rect(bx + 2, 44, 2, 6, false);
            }
            display_draw_text_centered_5x7(bx, 44, 20, fields[i], false);
        } else {
            display_draw_round_rect(bx, 42, 20, 10, true);
            display_draw_text_centered_5x7(bx, 44, 20, fields[i], true);
        }
    }
    ui_core_draw_footer_hint(ox, ui_runtime_is_alarm_editing() ? "UP/DN Adjust  OK Done" : "OK Select  BK Back");
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
        else if (e->id == KEY_ID_DOWN && ui_runtime_get_alarm_field() < 4U) ui_runtime_set_alarm_field((uint8_t)(ui_runtime_get_alarm_field() + 1U));
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
