#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_activity_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[24];

    (void)page;
    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Activity");
    ui_core_draw_card(ox + 8, 14, 112, 22, "TODAY");
    snprintf(line, sizeof(line), "%lu", (unsigned long)snap.activity.steps);
    display_draw_text_centered_5x7(ox, 22, 128, line, true);
    display_draw_progress_bar(ox + 18, 39, 92, 8, snap.activity.goal_percent, false);
    snprintf(line, sizeof(line), "G%lu  A%uMIN", (unsigned long)snap.activity.goal, snap.activity.active_minutes);
    display_draw_text_centered_5x7(ox, 48, 128, line, true);
    ui_core_draw_footer_hint(ox, "BK Back");
}

bool ui_page_activity_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}
