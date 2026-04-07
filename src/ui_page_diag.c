#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_diag_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[24];

    (void)page;
    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Diag");
    ui_core_draw_card(ox + 8, 14, 112, 22, snap.safe_mode_active ? "SAFE MODE" : "SYSTEM");
    snprintf(line, sizeof(line), "%s / %s", snap.reset_reason_name, snap.wake_reason_name);
    display_draw_text_centered_5x7(ox, 22, 128, line, true);
    snprintf(line, sizeof(line), "%s", snap.has_last_fault ? snap.last_fault_name : "NONE");
    ui_core_draw_kv_row(ox + 10, 39, 108, "FAULT", line);
    snprintf(line, sizeof(line), "%s", snap.wdt_last_checkpoint_result_name);
    snprintf(line, sizeof(line), "%s  %lu", snap.wdt_last_checkpoint_result_name, (unsigned long)snap.retained_max_loop_ms);
    ui_core_draw_kv_row(ox + 10, 47, 108, "WDT/LOOP", line);
    ui_core_draw_footer_hint(ox, snap.safe_mode_active ? "OK Clear Safe Mode" : "BK Back");
}

bool ui_page_diag_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    UiSystemSnapshot snap;

    (void)page;
    ui_get_system_snapshot(&snap);
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_LONG) && e->id == KEY_ID_OK && snap.safe_mode_active) {
        ui_request_clear_safe_mode();
        if (snap.safe_mode_can_clear) {
            ui_core_go(PAGE_WATCHFACE, 1, now_ms);
        }
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        if (snap.safe_mode_active) {
            return true;
        }
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}
