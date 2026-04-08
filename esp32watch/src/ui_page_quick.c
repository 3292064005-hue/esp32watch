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
    char subline[32];
    uint8_t next_alarm_idx = model_find_next_enabled_alarm();
    uint32_t now_ms = platform_time_now_ms();

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL) {
        return;
    }

    (void)network_sync_service_get_snapshot(&net_snapshot);
    (void)time_service_get_source_snapshot(&time_snapshot);

    ui_core_draw_header(ox, "Hub");
    ui_core_draw_card(ox + 8, 14, 112, 38, "");

    if (ui_runtime_get_quick_index() == 0U) {
        display_draw_text_centered_5x7(ox, 20, 128, "SYSTEM", true);
        snprintf(line, sizeof(line), "FACE %u  BR %u/4", domain_state.settings.watchface + 1U, domain_state.settings.brightness + 1U);
        snprintf(subline, sizeof(subline), "%s  %s",
                 domain_state.settings.auto_wake ? "RAISE ON" : "RAISE OFF",
                 domain_state.settings.auto_sleep ? "SLEEP ON" : "SLEEP OFF");
    } else if (ui_runtime_get_quick_index() == 1U) {
        uint8_t pct = (uint8_t)APP_CLAMP((domain_state.activity.goal == 0U) ? 0U : (domain_state.activity.steps * 100UL) / domain_state.activity.goal, 0U, 100U);

        display_draw_text_centered_5x7(ox, 20, 128, "ACTIVITY", true);
        snprintf(line, sizeof(line), "%lu / %lu", (unsigned long)domain_state.activity.steps, (unsigned long)domain_state.activity.goal);
        snprintf(subline, sizeof(subline), "%u%%  %u MIN  LV %u", pct, domain_state.activity.active_minutes, domain_state.activity.activity_level);
        display_draw_text_centered_5x7(ox, 31, 128, line, true);
        display_draw_text_centered_5x7(ox, 40, 128, subline, true);
        display_draw_progress_bar(ox + 22, 46, 84, 5, pct, false);
        ui_core_draw_scrollbar(ox + 121, 14, 38, 5, ui_runtime_get_quick_index());
        ui_core_draw_footer_hint(ox, "OK Open  BK Watch");
        return;
    } else if (ui_runtime_get_quick_index() == 2U) {
        display_draw_text_centered_5x7(ox, 20, 128, "ALARM", true);
        if (next_alarm_idx < APP_MAX_ALARMS) {
            const AlarmState *a = model_get_alarm(next_alarm_idx);
            snprintf(line, sizeof(line), "A%u  %02u:%02u", next_alarm_idx + 1U, a->hour, a->minute);
            snprintf(subline, sizeof(subline), "%s  %s", a->enabled ? "ARMED" : "OFF", a->ringing ? "RINGING" : "READY");
        } else {
            snprintf(line, sizeof(line), "NO ALARM");
            snprintf(subline, sizeof(subline), "OK TO CONFIGURE");
        }
    } else if (ui_runtime_get_quick_index() == 3U) {
        display_draw_text_centered_5x7(ox, 20, 128, "SENSOR", true);
        snprintf(line, sizeof(line), "%s  Q%u  CAL %u%%", runtime_state.sensor.online ? "ONLINE" : "OFFLINE", runtime_state.sensor.quality, runtime_state.sensor.calibration_progress);
        snprintf(subline, sizeof(subline), "P %d  R %d  M %d", runtime_state.sensor.pitch_deg, runtime_state.sensor.roll_deg, runtime_state.sensor.motion_score);
    } else {
        (void)net_snapshot;
        (void)time_snapshot;
        (void)now_ms;
        display_draw_text_centered_5x7(ox, 20, 128, "NETWORK", true);
        ui_status_compose_network_value(line, sizeof(line), subline, sizeof(subline));
    }

    display_draw_text_centered_5x7(ox, 31, 128, line, true);
    display_draw_text_centered_5x7(ox, 41, 128, subline, true);
    ui_core_draw_scrollbar(ox + 121, 14, 38, 5, ui_runtime_get_quick_index());
    ui_core_draw_footer_hint(ox, ui_runtime_get_quick_index() == 4U ? "OK Refresh  BK Watch" : "OK Open  BK Watch");
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
