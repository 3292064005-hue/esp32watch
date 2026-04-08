#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include "services/diag_service.h"
#include "web/web_wifi.h"
#include <stdio.h>

void ui_page_watchface_render(PageId page, int16_t ox)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    char tags[32];
    char line[32];
    char hint[32];
    uint8_t pct = 0U;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL) {
        return;
    }
    ui_core_draw_status_bar(ox);
    ui_status_compose_header_tags(tags, sizeof(tags));
    if (tags[0] != '\0') {
        display_draw_text_right_5x7(ox + 100, 2, tags, true);
    }

    if (domain_state.settings.watchface == 0U) {
        if (domain_state.time_valid) {
            display_draw_big_digit(ox + 6, 15, domain_state.now.hour / 10U, 2, true);
            display_draw_big_digit(ox + 26, 15, domain_state.now.hour % 10U, 2, true);
            display_draw_big_colon(ox + 46, 15, 2, true, true);
            display_draw_big_digit(ox + 58, 15, domain_state.now.minute / 10U, 2, true);
            display_draw_big_digit(ox + 78, 15, domain_state.now.minute % 10U, 2, true);

            if (domain_state.settings.show_seconds) {
                display_fill_round_rect(ox + 102, 19, 18, 10, true);
                snprintf(line, sizeof(line), "%02u", domain_state.now.second);
                display_draw_text_centered_5x7(ox + 102, 21, 18, line, false);
            }

            snprintf(line, sizeof(line), "%04u-%02u-%02u", domain_state.now.year, domain_state.now.month, domain_state.now.day);
            display_draw_text_centered_5x7(ox, 47, 128, line, true);
            snprintf(line, sizeof(line), "%s  %s",
                     web_wifi_connected() ? "WIFI" : "OFFLINE",
                     domain_state.alarm_ringing_index < APP_MAX_ALARMS ? "ALARM" : "READY");
            display_draw_text_centered_5x7(ox, 56, 128, line, true);
            snprintf(hint, sizeof(hint), "%s", domain_state.time_state == TIME_STATE_RECOVERED ? "OK Hub  BK Quick" : "OK Hub  BK Quick");
        } else {
            display_draw_text_centered_5x7(ox, 24, 128, "--:--", true);
            display_draw_text_centered_5x7(ox, 38, 128, "TIME NOT SET", true);
            snprintf(hint, sizeof(hint), "TIME UNSET  OK Settings");
        }
        ui_core_draw_footer_hint(ox, hint);
    } else if (domain_state.settings.watchface == 1U) {
        pct = (uint8_t)APP_CLAMP((domain_state.activity.goal == 0U) ? 0U : (domain_state.activity.steps * 100UL) / domain_state.activity.goal, 0U, 100U);

        ui_core_draw_card(ox + 10, 14, 108, 37, "ACTIVITY");
        if (domain_state.time_valid) {
            snprintf(line, sizeof(line), "%02u:%02u", domain_state.now.hour, domain_state.now.minute);
            display_draw_text_centered_5x7(ox, 19, 128, line, true);
            snprintf(line, sizeof(line), "%02u/%02u %s", domain_state.now.month, domain_state.now.day, ui_weekday_name(domain_state.now.weekday));
        } else {
            display_draw_text_centered_5x7(ox, 19, 128, "--:--", true);
            snprintf(line, sizeof(line), "TIME NOT SET");
        }
        display_draw_text_centered_5x7(ox, 28, 128, line, true);
        snprintf(line, sizeof(line), "%lu / %lu", (unsigned long)domain_state.activity.steps, (unsigned long)domain_state.activity.goal);
        display_draw_text_centered_5x7(ox, 37, 128, line, true);
        display_draw_progress_bar(ox + 22, 44, 84, 7, pct, false);
        snprintf(line, sizeof(line), "ACTIVE %u MIN  LV %u", domain_state.activity.active_minutes, domain_state.activity.activity_level);
        display_draw_text_centered_5x7(ox, 53, 128, line, true);
        snprintf(hint, sizeof(hint), "OK Hub  BK Quick");
        ui_core_draw_footer_hint(ox, hint);
    } else {
        ui_core_draw_card(ox + 8, 14, 112, 37, "HEALTH");
        if (domain_state.time_valid) {
            snprintf(line, sizeof(line), "%02u:%02u:%02u", domain_state.now.hour, domain_state.now.minute, domain_state.now.second);
        } else {
            snprintf(line, sizeof(line), "--:--:--");
        }
        display_draw_text_centered_5x7(ox, 19, 128, line, true);
        snprintf(line, sizeof(line), "%s Q%u", runtime_state.sensor.online ? "ON" : "OFF", runtime_state.sensor.quality);
        ui_core_draw_kv_row(ox + 10, 29, 108, "IMU", line);
        snprintf(line, sizeof(line), "%s", runtime_state.storage_crc_ok ? "OK" : "ATTN");
        ui_core_draw_kv_row(ox + 10, 37, 108, "STORE", line);
        snprintf(line, sizeof(line), "%s %u%%", diag_service_is_safe_mode_active() ? "SAFE" : "ACT", pct);
        ui_core_draw_kv_row(ox + 10, 45, 108, "STATE", line);
        ui_core_draw_footer_hint(ox, domain_state.time_valid ? "OK Hub  BK Quick" : "TIME UNSET");
    }
}

bool ui_page_watchface_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type == KEY_EVENT_SHORT) {
        if (e->id == KEY_ID_UP) ui_request_cycle_watchface(-1);
        else if (e->id == KEY_ID_DOWN) ui_request_cycle_watchface(1);
        else if (e->id == KEY_ID_OK) ui_core_go(PAGE_APPS, 1, now_ms);
        else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_QUICK, -1, now_ms);
        else return false;
        return true;
    }
    return false;
}
