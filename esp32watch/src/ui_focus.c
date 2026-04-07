#include "ui_internal.h"
#include "model.h"
#include <stddef.h>

UiFocusMode ui_focus_get_mode(void)
{
    ModelUiState ui_state;

    if (ui_runtime_is_sleeping_inline()) return UI_FOCUS_SLEEP;
    if (model_get_ui_state(&ui_state) != NULL && ui_state.popup != POPUP_NONE) return UI_FOCUS_POPUP;
    if (ui_runtime_is_settings_editing() || ui_runtime_is_alarm_editing()) return UI_FOCUS_EDIT;
    return UI_FOCUS_NAV;
}

bool ui_focus_is_editing(void)
{
    return ui_focus_get_mode() == UI_FOCUS_EDIT;
}
