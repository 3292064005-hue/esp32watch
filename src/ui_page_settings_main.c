#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

static const char * const settings_items[] = {
    "Bright", "RaiseWake", "AutoSleep", "DND", "Goal", "Face",
    "Seconds", "Anim", "Timeout", "Sens", "Calibrate", "Input Test",
    "Storage", "Time Set", "Restore"
};

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
    uint8_t start = (ui_runtime_get_settings_index() / 5U) * 5U;
    char line[32];
    (void)page;

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Settings");
    for (uint8_t i = 0; i < 5U; ++i) {
        uint8_t idx = start + i;
        bool sel;
        int16_t y;
        if (idx >= total) break;
        sel = idx == ui_runtime_get_settings_index();
        y = 16 + i * 9;
        if (sel) display_fill_round_rect(ox + 4, y - 1, 118, 8, true);
        switch (idx) {
            case 0: snprintf(line, sizeof(line), "%c Bright   %u", sel && ui_runtime_is_settings_editing() ? '*' : ' ', snap.settings.brightness + 1U); break;
            case 1: snprintf(line, sizeof(line), "  RaiseWake %s", snap.settings.auto_wake ? "ON" : "OFF"); break;
            case 2: snprintf(line, sizeof(line), "  AutoSleep %s", snap.settings.auto_sleep ? "ON" : "OFF"); break;
            case 3: snprintf(line, sizeof(line), "  DND      %s", snap.settings.dnd ? "ON" : "OFF"); break;
            case 4: snprintf(line, sizeof(line), "%c Goal     %lu", sel && ui_runtime_is_settings_editing() ? '*' : ' ', (unsigned long)snap.settings.goal); break;
            case 5: snprintf(line, sizeof(line), "%c Face     %u", sel && ui_runtime_is_settings_editing() ? '*' : ' ', snap.settings.watchface + 1U); break;
            case 6: snprintf(line, sizeof(line), "  Seconds  %s", snap.settings.show_seconds ? "ON" : "OFF"); break;
            case 7: snprintf(line, sizeof(line), "  Anim     %s", snap.settings.animations ? "ON" : "OFF"); break;
            case 8: snprintf(line, sizeof(line), "%c Timeout  %lus", sel && ui_runtime_is_settings_editing() ? '*' : ' ', (unsigned long)snap.settings.screen_timeout_s); break;
            case 9: snprintf(line, sizeof(line), "%c Sens     %u", sel && ui_runtime_is_settings_editing() ? '*' : ' ', snap.settings.sensor_sensitivity + 1U); break;
            case 10: snprintf(line, sizeof(line), "  Calibrate IMU"); break;
            case 11: snprintf(line, sizeof(line), "  Input Test"); break;
            case 12: snprintf(line, sizeof(line), "  Storage Info"); break;
            case 13: snprintf(line, sizeof(line), "  Time Set"); break;
            default: snprintf(line, sizeof(line), "  Restore Defaults"); break;
        }
        display_draw_text_5x7(ox + 8, y, line, !sel);
    }
    ui_core_draw_scrollbar(ox + 121, 16, 41, total, ui_runtime_get_settings_index());
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
                    {
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
