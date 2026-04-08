#include "ui.h"
#include "app_tuning.h"
#include "ui_internal.h"
#include "app_config.h"
#include "model.h"
#include "services/input_service.h"
#include "services/power_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include "power_policy.h"
#include "ui_page_registry.h"
#include "platform_api.h"

void ui_core_wake_internal(WakeReason reason)
{
    ui_runtime_set_sleeping(false);
    power_service_wake_with_reason(reason);
    ui_runtime_set_last_input_ms(platform_time_now_ms());
    model_note_manual_wake(ui_runtime_get_last_input_ms());
    ui_core_mark_dirty();
}

void ui_wake(void)
{
    ui_core_wake_internal(WAKE_REASON_MANUAL);
}

bool ui_is_sleeping(void)
{
    return ui_runtime_is_sleeping_inline();
}

void ui_tick(uint32_t now_ms)
{
    KeyEvent ev;
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    ModelUiState ui_state;

    while (input_service_pop_event(&ev)) {
        ui_runtime_set_last_input_ms(now_ms);
        model_note_user_activity();
        if (ui_runtime_is_sleeping_inline()) {
            ui_core_wake_internal(WAKE_REASON_KEY);
            continue;
        }
        if (ui_nav_policy_handle_global(&ev, now_ms)) {
            ui_core_mark_dirty();
         } else if (ui_core_handle_page_event(ui_runtime_get_current_page(), &ev, now_ms)) {
            ui_core_mark_dirty();
        }
    }

    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL || model_get_ui_state(&ui_state) == NULL) {
        return;
    }

    if (ui_runtime_is_sleeping_inline() && ui_state.popup != POPUP_NONE) {
        ui_core_wake_internal(WAKE_REASON_POPUP);
    }

    if (domain_state.settings.auto_wake && domain_state.activity.wrist_raise && ui_runtime_is_sleeping_inline()) {
        ui_core_wake_internal(WAKE_REASON_WRIST_RAISE);
    }

#if !(defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP)
    if (!ui_runtime_is_sleeping_inline() && domain_state.settings.auto_sleep) {
        PowerSleepBlocker blocker;
        bool can_auto_sleep = power_policy_can_auto_sleep(ui_runtime_get_current_page(),
                                                          ui_focus_is_editing(),
                                                          ui_state.popup,
                                                          domain_state.timer.running,
                                                          domain_state.stopwatch.running,
                                                          domain_state.alarm_ringing_index < APP_MAX_ALARMS,
                                                          storage_service_has_pending(),
                                                          diag_service_is_safe_mode_active(),
                                                          &blocker);
        (void)blocker;
        if (can_auto_sleep && (now_ms - ui_runtime_get_last_input_ms()) > runtime_state.screen_timeout_ms) {
            ui_request_sleep(SLEEP_REASON_AUTO_TIMEOUT);
        }
    }
#endif

    if (ui_runtime_is_animating() && (now_ms - ui_runtime_get_anim_start_ms()) >= app_tuning_manifest_get()->ui_anim_duration_ms) {
        ui_runtime_set_animating(false);
        ui_runtime_set_current_page(ui_runtime_get_to_page());
        ui_core_mark_dirty();
    }
}

bool ui_should_render(uint32_t now_ms)
{
    const UiPageOps *page;
    ModelDomainState domain_state;
    ModelUiState ui_state;

    if (ui_runtime_is_sleeping_inline()) return false;
    if (model_get_domain_state(&domain_state) == NULL || model_get_ui_state(&ui_state) == NULL) {
        return false;
    }
    if (ui_state.popup != ui_runtime_get_last_popup()) return true;
    if (ui_runtime_is_dirty()) return true;
    if (ui_runtime_is_animating()) return (now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_frame_ms;
    if (ui_state.popup != POPUP_NONE) return (now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_popup_refresh_ms;

    page = ui_page_registry_get(ui_runtime_get_current_page());
    switch (page->refresh_policy) {
        case UI_REFRESH_WATCHFACE:
            return (now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_watchface_refresh_ms;
        case UI_REFRESH_CARD:
            return (now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_card_refresh_ms;
        case UI_REFRESH_SENSOR:
            return (now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_sensor_refresh_ms;
        case UI_REFRESH_GAME:
            return (now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_game_refresh_ms;
        case UI_REFRESH_LIQUID:
            return (now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_liquid_refresh_ms;
        case UI_REFRESH_TIMER:
            return domain_state.timer.running && ((now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_timer_refresh_ms);
        case UI_REFRESH_STOPWATCH:
            return domain_state.stopwatch.running && ((now_ms - ui_runtime_get_last_render_ms()) >= app_tuning_manifest_get()->ui_stopwatch_refresh_ms);
        default:
            return false;
    }
}
