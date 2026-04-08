#include "ui_internal.h"
#include "model.h"

bool ui_nav_policy_handle_global(const KeyEvent *e, uint32_t now_ms)
{
    if (ui_focus_get_mode() == UI_FOCUS_POPUP) {
        return ui_popup_handle_event(e);
    }

    if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_BACK) {
        if (ui_runtime_get_current_page() == PAGE_SNAKE || ui_runtime_get_current_page() == PAGE_TETRIS) {
            return false;
        }
        if (ui_runtime_get_current_page() == PAGE_WATCHFACE) {
            ui_request_sleep(SLEEP_REASON_MANUAL);
            ui_core_mark_dirty();
            return true;
        }
        ui_runtime_set_settings_editing(false);
        ui_runtime_set_alarm_editing(false);
        ui_core_go(PAGE_WATCHFACE, -1, now_ms);
        return true;
    }

    return false;
}
