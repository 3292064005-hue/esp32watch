#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

void ui_page_watchface_render(PageId page, int16_t ox)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    char line[32];
    uint8_t pct;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL) {
        return;
    }
    ui_core_draw_status_bar(ox);

    if (domain_state.settings.watchface == 0U) {
        if (domain_state.time_valid) {
            display_draw_big_digit(ox + 8, 16, domain_state.now.hour / 10U, 2, true);
            display_draw_big_digit(ox + 28, 16, domain_state.now.hour % 10U, 2, true);
            display_draw_big_colon(ox + 48, 16, 2, true, true);
            display_draw_big_digit(ox + 60, 16, domain_state.now.minute / 10U, 2, true);
            display_draw_big_digit(ox + 80, 16, domain_state.now.minute % 10U, 2, true);
            snprintf(line, sizeof(line), "%04u-%02u-%02u %s", domain_state.now.year, domain_state.now.month, domain_state.now.day, ui_weekday_name(domain_state.now.weekday));
            display_draw_text_centered_5x7(ox, 50, 128, line, true);
            if (domain_state.time_state == TIME_STATE_RECOVERED) {
                display_draw_text_5x7(ox + 4, 58, "time synced", true);
            }
            if (domain_state.settings.show_seconds) {
                snprintf(line, sizeof(line), "%02u", domain_state.now.second);
                display_draw_text_5x7(ox + 108, 32, line, true);
            }
        } else {
            display_draw_text_centered_5x7(ox, 24, 128, "--:--", true);
            display_draw_text_centered_5x7(ox, 38, 128, "time not set", true);
            display_draw_text_centered_5x7(ox, 58, 128, "settings -> time set", true);
        }
    } else if (domain_state.settings.watchface == 1U) {
        pct = (uint8_t)APP_CLAMP((domain_state.activity.goal == 0U) ? 0U : (domain_state.activity.steps * 100UL) / domain_state.activity.goal, 0U, 100U);
        if (domain_state.time_valid) {
            snprintf(line, sizeof(line), "%02u:%02u", domain_state.now.hour, domain_state.now.minute);
            display_draw_text_centered_5x7(ox, 20, 128, line, true);
            snprintf(line, sizeof(line), "%04u/%02u/%02u", domain_state.now.year, domain_state.now.month, domain_state.now.day);
        } else {
            snprintf(line, sizeof(line), "--:--");
            display_draw_text_centered_5x7(ox, 20, 128, line, true);
            snprintf(line, sizeof(line), "time not set");
        }
        display_draw_text_centered_5x7(ox, 31, 128, line, true);
        if (domain_state.time_state == TIME_STATE_RECOVERED) display_draw_text_5x7(ox + 4, 32, "SYNC", true);
        snprintf(line, sizeof(line), "steps %lu/%lu", (unsigned long)domain_state.activity.steps, (unsigned long)domain_state.activity.goal);
        display_draw_text_centered_5x7(ox, 44, 128, line, true);
        display_draw_progress_bar(ox + 18, 55, 92, 7, pct, false);
        if (!domain_state.time_valid) display_draw_text_5x7(ox + 4, 56, "TIME?", true);
    } else {
        if (domain_state.time_valid) snprintf(line, sizeof(line), "%02u:%02u:%02u", domain_state.now.hour, domain_state.now.minute, domain_state.now.second);
        else snprintf(line, sizeof(line), "--:--:--");
        display_draw_text_centered_5x7(ox, 16, 128, line, true);
        snprintf(line, sizeof(line), "goal %lu  step %lu", (unsigned long)domain_state.activity.goal, (unsigned long)domain_state.activity.steps);
        display_draw_text_5x7(ox + 8, 30, line, true);
        snprintf(line, sizeof(line), "imu %s q%u", runtime_state.sensor.online ? "on" : "off", runtime_state.sensor.quality);
        display_draw_text_5x7(ox + 8, 40, line, true);
        snprintf(line, sizeof(line), "%s act %umin", domain_state.activity.motion_state >= MOTION_STATE_WALKING ? "walk" : "idle", domain_state.activity.active_minutes);
        display_draw_text_5x7(ox + 8, 50, line, true);
        display_draw_text_centered_5x7(ox, 58, 128, !domain_state.time_valid ? "time unset" : (domain_state.activity.wrist_raise ? "raise detected" : "OK apps  BK quick"), true);
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
