#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_diag_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];
    (void)page;

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Diag");

    snprintf(line, sizeof(line), "rst %s wake %s", snap.reset_reason_name, snap.wake_reason_name);
    display_draw_text_5x7(ox + 4, 12, line, true);

    snprintf(line, sizeof(line), "slp %s t %s", snap.sleep_reason_name, snap.time_state_name);
    display_draw_text_5x7(ox + 4, 21, line, true);

    if (snap.has_last_fault) {
        snprintf(line, sizeof(line), "flt %s %s c%u", snap.last_fault_name,
                 snap.last_fault_severity_name, (unsigned)snap.last_fault_count);
    } else {
        snprintf(line, sizeof(line), "flt none uiq %lu", (unsigned long)snap.ui_mutation_overflow_event_count);
    }
    display_draw_text_5x7(ox + 4, 30, line, true);

    snprintf(line, sizeof(line), "wdt %s/%s", snap.wdt_last_checkpoint_name,
             snap.wdt_last_checkpoint_result_name);
    display_draw_text_5x7(ox + 4, 39, line, true);

    if (snap.has_last_log) {
        snprintf(line, sizeof(line), "%s %u a%u", snap.last_log_name,
                 (unsigned)snap.last_log_value, (unsigned)snap.last_log_aux);
    } else {
        snprintf(line, sizeof(line), "no recent diag event");
    }
    display_draw_text_5x7(ox + 4, 48, line, true);

    if (snap.safe_mode_active) {
        snprintf(line, sizeof(line), "safe %s clr %s", snap.safe_mode_reason_name,
                 snap.safe_mode_can_clear ? "yes" : "no");
    } else {
        snprintf(line, sizeof(line), "prev p%u loop%lu", (unsigned)snap.retained_checkpoint,
                 (unsigned long)snap.retained_max_loop_ms);
    }
    display_draw_text_5x7(ox + 4, 57, line, true);
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
