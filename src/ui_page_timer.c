#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

void ui_page_timer_render(PageId page, int16_t ox)
{
    ModelDomainState domain_state;
    char line[24];

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }
    ui_core_draw_header(ox, "Timer");
    snprintf(line, sizeof(line), "%02lu:%02lu", (unsigned long)(domain_state.timer.remain_s / 60U), (unsigned long)(domain_state.timer.remain_s % 60U));
    display_draw_text_centered_5x7(ox, 18, 128, line, true);
    snprintf(line, sizeof(line), "preset %lus", (unsigned long)domain_state.timer.preset_s);
    display_draw_text_centered_5x7(ox, 29, 128, line, true);
    display_draw_progress_bar(ox + 16, 40, 96, 10,
                              (uint8_t)APP_CLAMP((domain_state.timer.preset_s == 0U) ? 0U : (domain_state.timer.remain_s * 100UL) / domain_state.timer.preset_s, 0U, 100U), false);
    snprintf(line, sizeof(line), "slot %u/%u", (unsigned)(domain_state.timer.preset_index + 1U), (unsigned)APP_TIMER_PRESET_COUNT);
    display_draw_text_centered_5x7(ox, 54, 128, line, true);
    display_draw_text_centered_5x7(ox, 61, 128, domain_state.timer.running ? "OK pause DN reset" : "UP/DN tune BK preset", true);
}

bool ui_page_timer_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    ModelDomainState domain_state;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return false;
    }
    if (e->type == KEY_EVENT_SHORT) {
        if (e->id == KEY_ID_OK) ui_request_timer_toggle(now_ms);
        else if (e->id == KEY_ID_UP) ui_request_timer_adjust_seconds(10);
        else if (e->id == KEY_ID_DOWN) ui_request_timer_adjust_seconds(-10);
        else if (e->id == KEY_ID_BACK && !domain_state.timer.running) ui_request_timer_cycle_preset(1);
        else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_APPS, -1, now_ms);
        else return false;
        return true;
    } else if ((e->type == KEY_EVENT_LONG || e->type == KEY_EVENT_REPEAT) && !domain_state.timer.running) {
        if (e->id == KEY_ID_UP) ui_request_timer_adjust_seconds(60);
        else if (e->id == KEY_ID_DOWN) ui_request_timer_adjust_seconds(-60);
        else if (e->id == KEY_ID_BACK) ui_request_timer_cycle_preset(1);
        else return false;
        return true;
    }
    return false;
}
