#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

static const char * const settings_items[] = {
    "Bright", "RaiseWake", "AutoSleep", "DND", "Goal", "Face",
    "Seconds", "Anim", "Timeout", "Sens", "Calibrate", "Input Test",
    "Storage", "Time Set", "Reset App"
};

static const char *setting_label(uint8_t idx)
{
    switch (idx) {
        case 0U: return "Brightness";
        case 1U: return "Raise Wake";
        case 2U: return "Auto Sleep";
        case 3U: return "DND";
        case 4U: return "Step Goal";
        case 5U: return "Watchface";
        case 6U: return "Seconds";
        case 7U: return "Motion";
        case 8U: return "Timeout";
        case 9U: return "IMU Sense";
        case 10U: return "Calibrate";
        case 11U: return "Input Test";
        case 12U: return "Storage";
        case 13U: return "Time Set";
        default: return "Reset App";
    }
}

static void setting_value_string(uint8_t idx, const UiSystemSnapshot *snap, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0U || snap == NULL) {
        return;
    }

    switch (idx) {
        case 0U: snprintf(out, out_size, "%u/4", snap->settings.brightness + 1U); break;
        case 1U: snprintf(out, out_size, "%s", snap->settings.auto_wake ? "ON" : "OFF"); break;
        case 2U: snprintf(out, out_size, "%s", snap->settings.auto_sleep ? "ON" : "OFF"); break;
        case 3U: snprintf(out, out_size, "%s", snap->settings.dnd ? "ON" : "OFF"); break;
        case 4U: snprintf(out, out_size, "%luk", (unsigned long)(snap->settings.goal / 1000UL)); break;
        case 5U: snprintf(out, out_size, "F%u", snap->settings.watchface + 1U); break;
        case 6U: snprintf(out, out_size, "%s", snap->settings.show_seconds ? "ON" : "OFF"); break;
        case 7U: snprintf(out, out_size, "%s", snap->settings.animations ? "ON" : "OFF"); break;
        case 8U: snprintf(out, out_size, "%lus", (unsigned long)snap->settings.screen_timeout_s); break;
        case 9U: snprintf(out, out_size, "L%u", snap->settings.sensor_sensitivity + 1U); break;
        case 10U: snprintf(out, out_size, "RUN"); break;
        case 11U: snprintf(out, out_size, "OPEN"); break;
        case 12U: snprintf(out, out_size, "INFO"); break;
        case 13U: snprintf(out, out_size, "SET"); break;
        default: snprintf(out, out_size, "NOW"); break;
    }
}

static void change_setting(int8_t delta)
{
    UiSystemSnapshot snap;

    ui_get_system_snapshot(&snap);
    switch (ui_runtime_get_settings_index()) {
        case 0U: ui_request_set_brightness((uint8_t)APP_CLAMP((int)snap.settings.brightness + delta, 0, 3)); break;
        case 4U: ui_request_set_goal((uint32_t)APP_CLAMP((int)snap.settings.goal + delta * 500, 1000, 30000)); break;
        case 5U: ui_request_cycle_watchface((int8_t)(delta > 0 ? 1 : -1)); break;
        case 8U: ui_request_cycle_screen_timeout((int8_t)(delta > 0 ? 1 : -1)); break;
        case 9U: ui_request_set_sensor_sensitivity((uint8_t)APP_CLAMP((int)snap.settings.sensor_sensitivity + delta, 0, 2)); break;
        default: break;
    }
}

void ui_page_settings_main_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    uint8_t total = (uint8_t)(sizeof(settings_items) / sizeof(settings_items[0]));
    uint8_t start = (ui_runtime_get_settings_index() / 4U) * 4U;

    (void)page;
    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Settings");
    for (uint8_t i = 0; i < 4U; ++i) {
        uint8_t idx = start + i;
        char value[16];

        if (idx >= total) break;
        setting_value_string(idx, &snap, value, sizeof(value));
        ui_core_draw_list_item(ox, 14 + i * 10, 110, setting_label(idx), value,
                               idx == ui_runtime_get_settings_index(),
                               idx >= 10U || (idx == ui_runtime_get_settings_index() && ui_runtime_is_settings_editing()));
    }
    ui_core_draw_scrollbar(ox + 121, 14, 40, total, ui_runtime_get_settings_index());
    ui_core_draw_footer_hint(ox, ui_runtime_is_settings_editing() ? "UP/DN Adjust  OK Done" : "OK Edit/Open  BK Back");
}

bool ui_page_settings_main_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    UiSystemSnapshot snap;
    uint8_t total = (uint8_t)(sizeof(settings_items) / sizeof(settings_items[0]));

    (void)page;
    ui_get_system_snapshot(&snap);
    if (ui_runtime_is_settings_editing()) {
        if (e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT || e->type == KEY_EVENT_LONG) {
            if (e->id == KEY_ID_UP) change_setting(1);
            else if (e->id == KEY_ID_DOWN) change_setting(-1);
            else if (e->id == KEY_ID_OK || e->id == KEY_ID_BACK) ui_runtime_set_settings_editing(false);
            else return false;
            return true;
        }
    } else if (e->type == KEY_EVENT_SHORT) {
        if (e->id == KEY_ID_UP && ui_runtime_get_settings_index() > 0U) ui_runtime_set_settings_index((uint8_t)(ui_runtime_get_settings_index() - 1U));
        else if (e->id == KEY_ID_DOWN && ui_runtime_get_settings_index() + 1U < total) ui_runtime_set_settings_index((uint8_t)(ui_runtime_get_settings_index() + 1U));
        else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_APPS, -1, now_ms);
        else if (e->id == KEY_ID_OK) {
            if (ui_runtime_get_settings_index() == 0U || ui_runtime_get_settings_index() == 4U || ui_runtime_get_settings_index() == 5U || ui_runtime_get_settings_index() == 8U || ui_runtime_get_settings_index() == 9U) {
                ui_runtime_set_settings_editing(true);
            } else if (ui_runtime_get_settings_index() == 1U) {
                ui_request_set_auto_wake(!snap.settings.auto_wake);
            } else if (ui_runtime_get_settings_index() == 2U) {
                ui_request_set_auto_sleep(!snap.settings.auto_sleep);
            } else if (ui_runtime_get_settings_index() == 3U) {
                ui_request_set_dnd(!snap.settings.dnd);
            } else if (ui_runtime_get_settings_index() == 6U) {
                ui_request_set_show_seconds(!snap.settings.show_seconds);
            } else if (ui_runtime_get_settings_index() == 7U) {
                ui_request_set_animations(!snap.settings.animations);
            } else if (ui_runtime_get_settings_index() == 10U) {
                ui_request_sensor_calibration();
                ui_core_go(PAGE_CALIBRATION, 1, now_ms);
            } else if (ui_runtime_get_settings_index() == 11U) {
                ui_core_go(PAGE_INPUTTEST, 1, now_ms);
            } else if (ui_runtime_get_settings_index() == 12U) {
                ui_core_go(PAGE_STORAGE, 1, now_ms);
            } else if (ui_runtime_get_settings_index() == 13U) {
                if (snap.settings.time_valid) {
                    ui_runtime_set_edit_time(&snap.settings.current_time);
                } else {
                    DateTime edit_time = {0};
                    edit_time.year = 2024U;
                    edit_time.month = 1U;
                    edit_time.day = 1U;
                    edit_time.weekday = 1U;
                    edit_time.hour = 0U;
                    edit_time.minute = 0U;
                    edit_time.second = 0U;
                    ui_runtime_set_edit_time(&edit_time);
                }
                ui_runtime_set_time_field(0U);
                ui_core_go(PAGE_TIMESET, 1, now_ms);
            } else {
                ui_request_restore_defaults();
            }
        } else return false;
        return true;
    }
    return false;
}
