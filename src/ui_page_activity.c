#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_activity_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];
    (void)page;

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Activity");
    snprintf(line, sizeof(line), "steps %lu", (unsigned long)snap.activity.steps);
    display_draw_text_centered_5x7(ox, 18, 128, line, true);
    snprintf(line, sizeof(line), "goal %lu  %u%%", (unsigned long)snap.activity.goal, snap.activity.goal_percent);
    display_draw_text_centered_5x7(ox, 29, 128, line, true);
    display_draw_progress_bar(ox + 16, 40, 96, 10, snap.activity.goal_percent, false);
    snprintf(line, sizeof(line), "lvl %u state %u active %u",
             snap.activity.activity_level,
             snap.activity.motion_state,
             snap.activity.active_minutes);
    display_draw_text_centered_5x7(ox, 55, 128, line, true);
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
