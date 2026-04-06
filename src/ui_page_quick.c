#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include "services/network_sync_service.h"
#include "services/time_service.h"
#include <stdio.h>

void ui_page_quick_render(PageId page, int16_t ox)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    NetworkSyncSnapshot net_snapshot;
    TimeSourceSnapshot time_snapshot;
    char line[32];
    uint8_t next_alarm_idx = model_find_next_enabled_alarm();
    uint32_t now_ms = platform_time_now_ms();

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL) {
        return;
    }
    (void)network_sync_service_get_snapshot(&net_snapshot);
    (void)time_service_get_source_snapshot(&time_snapshot);
    ui_core_draw_header(ox, "Quick");
    display_draw_round_rect(ox + 8, 16, 112, 40, true);
    if (ui_runtime_get_quick_index() == 0U) {
        display_draw_text_centered_5x7(ox, 22, 128, "System", true);
        snprintf(line, sizeof(line), "face %u bright %u", domain_state.settings.watchface + 1U, domain_state.settings.brightness + 1U);
        display_draw_text_centered_5x7(ox, 34, 128, line, true);
        display_draw_text_centered_5x7(ox, 46, 128, domain_state.settings.auto_wake ? "raise wake on" : "raise wake off", true);
    } else if (ui_runtime_get_quick_index() == 1U) {
        uint8_t pct = (uint8_t)APP_CLAMP((domain_state.activity.goal == 0U) ? 0U : (domain_state.activity.steps * 100UL) / domain_state.activity.goal, 0U, 100U);
        display_draw_text_centered_5x7(ox, 22, 128, "Activity", true);
        snprintf(line, sizeof(line), "%lu / %lu", (unsigned long)domain_state.activity.steps, (unsigned long)domain_state.activity.goal);
        display_draw_text_centered_5x7(ox, 34, 128, line, true);
        display_draw_progress_bar(ox + 20, 46, 88, 8, pct, false);
    } else if (ui_runtime_get_quick_index() == 2U) {
        display_draw_text_centered_5x7(ox, 22, 128, "Alarm", true);
        if (next_alarm_idx < APP_MAX_ALARMS) {
            const AlarmState *a = model_get_alarm(next_alarm_idx);
            snprintf(line, sizeof(line), "A%u %s %02u:%02u", next_alarm_idx + 1U, a->enabled ? "on" : "off", a->hour, a->minute);
            display_draw_text_centered_5x7(ox, 34, 128, line, true);
            display_draw_text_centered_5x7(ox, 46, 128, a->ringing ? "ringing" : "next ready", true);
        } else {
            display_draw_text_centered_5x7(ox, 34, 128, "no alarm enabled", true);
            display_draw_text_centered_5x7(ox, 46, 128, "OK to configure", true);
        }
    } else if (ui_runtime_get_quick_index() == 3U) {
        display_draw_text_centered_5x7(ox, 22, 128, "Sensor", true);
        snprintf(line, sizeof(line), "norm %d base %d", runtime_state.sensor.accel_norm_mg, runtime_state.sensor.baseline_mg);
        display_draw_text_centered_5x7(ox, 34, 128, line, true);
        snprintf(line, sizeof(line), "%s q%u", runtime_state.sensor.online ? "imu online" : "imu offline", runtime_state.sensor.quality);
        display_draw_text_centered_5x7(ox, 46, 128, line, true);
    } else {
        uint32_t age_s = (net_snapshot.last_weather_sync_ms == 0U || now_ms < net_snapshot.last_weather_sync_ms) ?
            0U : ((now_ms - net_snapshot.last_weather_sync_ms) / 1000UL);
        display_draw_text_centered_5x7(ox, 22, 128, "Weather", true);
        if (net_snapshot.weather_valid) {
            snprintf(line, sizeof(line), "%s %d.%dC",
                     net_snapshot.location_name,
                     (int)(net_snapshot.temperature_tenths_c / 10),
                     (int)((net_snapshot.temperature_tenths_c < 0 ? -net_snapshot.temperature_tenths_c : net_snapshot.temperature_tenths_c) % 10));
            display_draw_text_centered_5x7(ox, 32, 128, line, true);
            display_draw_text_centered_5x7(ox, 42, 128, net_snapshot.weather_text, true);
            snprintf(line, sizeof(line), "%s %lus",
                     time_service_source_name(time_snapshot.source),
                     (unsigned long)age_s);
            display_draw_text_centered_5x7(ox, 52, 128, line, true);
        } else if (net_snapshot.wifi_connected) {
            display_draw_text_centered_5x7(ox, 34, 128, "syncing weather", true);
            display_draw_text_centered_5x7(ox, 46, 128, "OK refresh now", true);
        } else {
            display_draw_text_centered_5x7(ox, 34, 128, "wifi offline", true);
            display_draw_text_centered_5x7(ox, 46, 128, "connect to sync", true);
        }
    }
    ui_core_draw_scrollbar(ox + 121, 17, 38, 5, ui_runtime_get_quick_index());
}

bool ui_page_quick_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type != KEY_EVENT_SHORT) return false;
    if (e->id == KEY_ID_UP) ui_runtime_set_quick_index((uint8_t)((ui_runtime_get_quick_index() + 4U) % 5U));
    else if (e->id == KEY_ID_DOWN) ui_runtime_set_quick_index((uint8_t)((ui_runtime_get_quick_index() + 1U) % 5U));
    else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_WATCHFACE, 1, now_ms);
    else if (e->id == KEY_ID_OK) {
        PageId target = PAGE_SETTINGS;
        if (ui_runtime_get_quick_index() == 1U) target = PAGE_ACTIVITY;
        else if (ui_runtime_get_quick_index() == 2U) target = PAGE_ALARM;
        else if (ui_runtime_get_quick_index() == 3U) target = PAGE_SENSOR;
        else if (ui_runtime_get_quick_index() == 4U) {
            network_sync_service_request_refresh();
            return true;
        }
        ui_core_go(target, 1, now_ms);
    } else return false;
    return true;
}

