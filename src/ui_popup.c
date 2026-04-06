#include "ui_internal.h"
#include "display.h"
#include "model.h"
#include "ui_page_registry.h"
#include <stdio.h>

void ui_popup_render(void)
{
    ModelDomainState domain_state;
    ModelUiState ui_state;
    PopupType popup;
    char line[24];
    bool page_allows_popup = ui_page_registry_allows_popup(ui_runtime_get_current_page());

    if (model_get_domain_state(&domain_state) == NULL || model_get_ui_state(&ui_state) == NULL) {
        return;
    }
    popup = ui_state.popup;
    if (popup == POPUP_NONE) return;
    if (!page_allows_popup && popup != POPUP_ALARM) return;
    display_fill_round_rect(12, 16, 104, 34, true);
    if (popup == POPUP_ALARM) {
        display_draw_text_centered_5x7(12, 22, 104, "ALARM", false);
        if (domain_state.alarm_ringing_index < APP_MAX_ALARMS) {
            snprintf(line, sizeof(line), "A%u %02u:%02u", domain_state.alarm_ringing_index + 1U, domain_state.selected_alarm.hour, domain_state.selected_alarm.minute);
            display_draw_text_centered_5x7(12, 30, 104, line, false);
        }
        display_draw_text_centered_5x7(12, 40, 104, "OK off  BK snooze", false);
    } else if (popup == POPUP_TIMER_DONE) {
        display_draw_text_centered_5x7(12, 22, 104, "TIMER DONE", false);
        display_draw_text_centered_5x7(12, 34, 104, "OK dismiss", false);
    } else if (popup == POPUP_LOW_BATTERY) {
        display_draw_text_centered_5x7(12, 22, 104, "LOW BATTERY", false);
        display_draw_text_centered_5x7(12, 34, 104, "Please charge", false);
    } else if (popup == POPUP_GOAL_REACHED) {
        display_draw_text_centered_5x7(12, 22, 104, "GOAL REACHED", false);
        display_draw_text_centered_5x7(12, 34, 104, "Great job", false);
    } else {
        display_draw_text_centered_5x7(12, 22, 104, "SENSOR FAULT", false);
        display_draw_text_centered_5x7(12, 34, 104, "OK dismiss", false);
    }
}

bool ui_popup_handle_event(const KeyEvent *e)
{
    ModelUiState ui_state;

    if (e->type != KEY_EVENT_SHORT && e->type != KEY_EVENT_LONG) return false;
    if (e->id == KEY_ID_OK) {
        model_ack_popup();
        return true;
    }
    if (model_get_ui_state(&ui_state) == NULL) {
        return false;
    }
    if (e->id == KEY_ID_BACK && ui_state.popup == POPUP_ALARM) {
        model_snooze_alarm();
        return true;
    }
    if (e->id == KEY_ID_BACK) {
        model_ack_popup();
        return true;
    }
    return false;
}
