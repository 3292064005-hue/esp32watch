#include "ui_internal.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

void ui_page_stopwatch_render(PageId page, int16_t ox)
{
    ModelDomainState domain_state;
    char line[24];
    uint32_t total_cs;
    uint8_t show;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }

    total_cs = domain_state.stopwatch.elapsed_ms / 10U;
    ui_core_draw_header(ox, "Stopwatch");
    ui_core_draw_card(ox + 8, 14, 112, 22, domain_state.stopwatch.running ? "RUNNING" : "READY");
    snprintf(line, sizeof(line), "%02lu:%02lu.%02lu",
             (unsigned long)((total_cs / 100U) / 60U),
             (unsigned long)((total_cs / 100U) % 60U),
             (unsigned long)(total_cs % 100U));
    display_draw_text_centered_5x7(ox, 22, 128, line, true);

    show = domain_state.stopwatch.lap_count > 2U ? 2U : domain_state.stopwatch.lap_count;
    for (uint8_t i = 0; i < show; ++i) {
        uint32_t lap = domain_state.stopwatch.laps[domain_state.stopwatch.lap_count - 1U - i] / 10U;

        snprintf(line, sizeof(line), "%02lu:%02lu.%02lu",
                 (unsigned long)((lap / 100U) / 60U),
                 (unsigned long)((lap / 100U) % 60U),
                 (unsigned long)(lap % 100U));
        ui_core_draw_kv_row(ox + 10, 39 + i * 8, 108, i == 0U ? "LAP 1" : "LAP 2", line);
    }
    ui_core_draw_footer_hint(ox, domain_state.stopwatch.running ? "OK Pause  BK Lap" : "OK Start  DN Reset");
}

bool ui_page_stopwatch_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    ModelDomainState domain_state;

    (void)page;
    if (model_get_domain_state(&domain_state) == NULL || e->type != KEY_EVENT_SHORT) return false;
    if (e->id == KEY_ID_OK) ui_request_stopwatch_toggle(now_ms);
    else if (e->id == KEY_ID_DOWN) ui_request_stopwatch_reset();
    else if (e->id == KEY_ID_BACK && domain_state.stopwatch.running) ui_request_stopwatch_lap();
    else if (e->id == KEY_ID_BACK) ui_core_go(PAGE_APPS, -1, now_ms);
    else return false;
    return true;
}
