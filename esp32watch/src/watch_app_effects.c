#include "watch_app_internal.h"
#include "app_command.h"
#include "app_config.h"
#include "model.h"
#include "services/activity_service.h"
#include "services/diag_service.h"
#include "services/display_service.h"
#include "services/power_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/storage_facade.h"
#include "ui.h"
#include "ui_internal.h"
#include "platform_api.h"
#include <stddef.h>
#include <string.h>

static void watch_app_sleep_now(SleepReason reason, WatchAppSleepRequestState *sleep_request)
{
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)reason;
    if (sleep_request != NULL) {
        sleep_request->pending = false;
        sleep_request->reason = SLEEP_REASON_NONE;
        sleep_request->requested_at_ms = 0U;
    }
    ui_runtime_set_sleeping(false);
    power_service_set_sleeping(false);
    ui_core_mark_dirty();
#else
    if (sleep_request != NULL) {
        sleep_request->pending = false;
        sleep_request->reason = SLEEP_REASON_NONE;
        sleep_request->requested_at_ms = 0U;
    }
    ui_runtime_set_sleeping(true);
    power_service_request_sleep(reason);
    ui_core_mark_dirty();
#endif
}

static void watch_app_request_sleep(SleepReason reason, WatchAppSleepRequestState *sleep_request)
{
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)reason;
    (void)sleep_request;
    return;
#else
    if (sleep_request == NULL || (reason != SLEEP_REASON_MANUAL && reason != SLEEP_REASON_AUTO_TIMEOUT)) {
        return;
    }

    if (storage_service_has_pending() || storage_service_is_transaction_active()) {
        storage_service_request_flush_before_sleep();
        sleep_request->pending = true;
        sleep_request->reason = reason;
        sleep_request->requested_at_ms = platform_time_now_ms();
        return;
    }

    watch_app_sleep_now(reason, sleep_request);
#endif
}

/**
 * @brief Translate a UI mutation descriptor into the shared application command shape.
 *
 * @param[in] mutation UI-side mutation descriptor.
 * @param[out] out Shared command descriptor populated on success.
 * @return true when the mutation maps to a supported command; false when the descriptor is NULL, empty, or unsupported.
 * @throws None.
 * @boundary_behavior Returns false without mutating @p out when either pointer is NULL or the mutation type is not mapped.
 */
static bool watch_app_command_from_ui_mutation(const UiModelMutation *mutation, AppCommand *out)
{
    if (mutation == NULL || out == NULL || mutation->type == UI_MODEL_MUTATION_NONE) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->source = APP_COMMAND_SOURCE_UI;

    switch (mutation->type) {
        case UI_MODEL_MUTATION_SET_BRIGHTNESS:
            out->type = APP_COMMAND_SET_BRIGHTNESS;
            out->data.u8 = mutation->data.u8;
            return true;
        case UI_MODEL_MUTATION_SET_GOAL:
            out->type = APP_COMMAND_SET_GOAL;
            out->data.u32 = mutation->data.u32;
            return true;
        case UI_MODEL_MUTATION_CYCLE_WATCHFACE:
            out->type = APP_COMMAND_CYCLE_WATCHFACE;
            out->data.delta_i8 = mutation->data.delta_i8;
            return true;
        case UI_MODEL_MUTATION_CYCLE_SCREEN_TIMEOUT:
            out->type = APP_COMMAND_CYCLE_SCREEN_TIMEOUT;
            out->data.delta_i8 = mutation->data.delta_i8;
            return true;
        case UI_MODEL_MUTATION_SET_SENSOR_SENSITIVITY:
            out->type = APP_COMMAND_SET_SENSOR_SENSITIVITY;
            out->data.u8 = mutation->data.u8;
            return true;
        case UI_MODEL_MUTATION_SET_AUTO_WAKE:
            out->type = APP_COMMAND_SET_AUTO_WAKE;
            out->data.enabled = mutation->data.enabled;
            return true;
        case UI_MODEL_MUTATION_SET_AUTO_SLEEP:
            out->type = APP_COMMAND_SET_AUTO_SLEEP;
            out->data.enabled = mutation->data.enabled;
            return true;
        case UI_MODEL_MUTATION_SET_DND:
            out->type = APP_COMMAND_SET_DND;
            out->data.enabled = mutation->data.enabled;
            return true;
        case UI_MODEL_MUTATION_SET_SHOW_SECONDS:
            out->type = APP_COMMAND_SET_SHOW_SECONDS;
            out->data.enabled = mutation->data.enabled;
            return true;
        case UI_MODEL_MUTATION_SET_ANIMATIONS:
            out->type = APP_COMMAND_SET_ANIMATIONS;
            out->data.enabled = mutation->data.enabled;
            return true;
        case UI_MODEL_MUTATION_RESTORE_DEFAULTS:
            out->type = APP_COMMAND_RESET_APP_STATE;
            return true;
        case UI_MODEL_MUTATION_SELECT_ALARM_OFFSET:
            out->type = APP_COMMAND_SELECT_ALARM_OFFSET;
            out->data.delta_i8 = mutation->data.delta_i8;
            return true;
        case UI_MODEL_MUTATION_SET_ALARM_ENABLED_AT:
            out->type = APP_COMMAND_SET_ALARM_ENABLED_AT;
            out->data.alarm_enabled.index = mutation->data.alarm_enabled.index;
            out->data.alarm_enabled.enabled = mutation->data.alarm_enabled.enabled;
            return true;
        case UI_MODEL_MUTATION_SET_ALARM_TIME_AT:
            out->type = APP_COMMAND_SET_ALARM_TIME_AT;
            out->data.alarm_time.index = mutation->data.alarm_time.index;
            out->data.alarm_time.hour = mutation->data.alarm_time.hour;
            out->data.alarm_time.minute = mutation->data.alarm_time.minute;
            return true;
        case UI_MODEL_MUTATION_SET_ALARM_REPEAT_MASK_AT:
            out->type = APP_COMMAND_SET_ALARM_REPEAT_MASK_AT;
            out->data.alarm_repeat.index = mutation->data.alarm_repeat.index;
            out->data.alarm_repeat.repeat_mask = mutation->data.alarm_repeat.repeat_mask;
            return true;
        case UI_MODEL_MUTATION_STOPWATCH_TOGGLE:
            out->type = APP_COMMAND_STOPWATCH_TOGGLE;
            out->data.now_ms = mutation->data.now_ms;
            return true;
        case UI_MODEL_MUTATION_STOPWATCH_RESET:
            out->type = APP_COMMAND_STOPWATCH_RESET;
            return true;
        case UI_MODEL_MUTATION_STOPWATCH_LAP:
            out->type = APP_COMMAND_STOPWATCH_LAP;
            return true;
        case UI_MODEL_MUTATION_TIMER_TOGGLE:
            out->type = APP_COMMAND_TIMER_TOGGLE;
            out->data.now_ms = mutation->data.now_ms;
            return true;
        case UI_MODEL_MUTATION_TIMER_ADJUST_SECONDS:
            out->type = APP_COMMAND_TIMER_ADJUST_SECONDS;
            out->data.delta_i32 = mutation->data.delta_i32;
            return true;
        case UI_MODEL_MUTATION_TIMER_CYCLE_PRESET:
            out->type = APP_COMMAND_TIMER_CYCLE_PRESET;
            out->data.delta_i8 = mutation->data.delta_i8;
            return true;
        case UI_MODEL_MUTATION_SET_DATETIME:
            out->type = APP_COMMAND_SET_DATETIME;
            out->data.date_time = mutation->data.date_time;
            return true;
        case UI_MODEL_MUTATION_SET_GAME_HIGH_SCORE:
            out->type = APP_COMMAND_SET_GAME_HIGH_SCORE;
            out->data.game_high_score.game_id = mutation->data.game_high_score.game_id;
            out->data.game_high_score.score = mutation->data.game_high_score.score;
            return true;
        case UI_MODEL_MUTATION_NONE:
        default:
            return false;
    }
}

void watch_app_apply_model_runtime_requests(uint8_t *last_sensor_sensitivity)
{
    StorageCommitReason reason = STORAGE_COMMIT_REASON_NONE;
    uint32_t flags = model_consume_runtime_requests(&reason);
    ModelDomainState domain_state;

    if (flags == 0U || model_get_domain_state(&domain_state) == NULL) {
        return;
    }

    if ((flags & MODEL_RUNTIME_REQUEST_APPLY_BRIGHTNESS) != 0U) {
        display_service_apply_settings(&domain_state.settings);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS) != 0U) {
        sensor_service_set_sensitivity(domain_state.settings.sensor_sensitivity);
        if (last_sensor_sensitivity != NULL) {
            *last_sensor_sensitivity = domain_state.settings.sensor_sensitivity;
        }
    }

    if ((flags & MODEL_RUNTIME_REQUEST_CLEAR_SENSOR_CALIBRATION) != 0U) {
        sensor_service_clear_calibration();
        storage_facade_clear_sensor_calibration();
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STAGE_SETTINGS) != 0U) {
        storage_facade_save_settings(&domain_state.settings);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STAGE_ALARMS) != 0U) {
        storage_facade_save_alarms(domain_state.alarms, APP_MAX_ALARMS);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STAGE_GAME_STATS) != 0U) {
        storage_facade_save_game_stats(&domain_state.game_stats);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STORAGE_COMMIT) != 0U) {
        storage_service_request_commit(reason);
    }
}

void watch_app_apply_ui_actions(WatchAppSleepRequestState *sleep_request, uint8_t *last_sensor_sensitivity)
{
    uint32_t actions = ui_consume_actions();
    UiModelMutation mutation;
    ModelDomainState domain_state;

    if ((actions & UI_ACTION_SENSOR_REINIT) != 0U) {
        AppCommand command = {.source = APP_COMMAND_SOURCE_UI, .type = APP_COMMAND_SENSOR_REINIT};
        (void)app_command_execute(&command, last_sensor_sensitivity);
    }
    if ((actions & UI_ACTION_SENSOR_CALIBRATION) != 0U) {
        AppCommand command = {.source = APP_COMMAND_SOURCE_UI, .type = APP_COMMAND_SENSOR_CALIBRATION};
        (void)app_command_execute(&command, last_sensor_sensitivity);
    }
    if ((actions & UI_ACTION_STORAGE_MANUAL_FLUSH) != 0U) {
        AppCommand command = {.source = APP_COMMAND_SOURCE_UI, .type = APP_COMMAND_STORAGE_MANUAL_FLUSH};
        (void)app_command_execute(&command, last_sensor_sensitivity);
    }
    if ((actions & UI_ACTION_CLEAR_SAFE_MODE) != 0U) {
        AppCommand command = {.source = APP_COMMAND_SOURCE_UI, .type = APP_COMMAND_CLEAR_SAFE_MODE};
        (void)app_command_execute(&command, last_sensor_sensitivity);
    }
    if ((actions & UI_ACTION_SYNC_SENSOR_SETTINGS) != 0U &&
        model_get_domain_state(&domain_state) != NULL) {
        sensor_service_set_sensitivity(domain_state.settings.sensor_sensitivity);
        if (last_sensor_sensitivity != NULL) {
            *last_sensor_sensitivity = domain_state.settings.sensor_sensitivity;
        }
    }

    while (ui_consume_model_mutation(&mutation)) {
        AppCommand command;
        if (watch_app_command_from_ui_mutation(&mutation, &command)) {
            (void)app_command_execute(&command, last_sensor_sensitivity);
        }
    }

    /*
     * UI-originated mutations can schedule runtime side effects such as staged
     * storage writes, brightness application, and sensor resynchronization.
     * Flush those requests in the same main-loop iteration so sleep/flush
     * arbitration observes the updated runtime state without a one-loop delay.
     */
    watch_app_apply_model_runtime_requests(last_sensor_sensitivity);

#if !(defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP)
    if ((actions & UI_ACTION_SLEEP_MANUAL) != 0U) {
        watch_app_request_sleep(SLEEP_REASON_MANUAL, sleep_request);
    } else if ((actions & UI_ACTION_SLEEP_AUTO) != 0U) {
        watch_app_request_sleep(SLEEP_REASON_AUTO_TIMEOUT, sleep_request);
    }
#else
    (void)actions;
#endif
}

void watch_app_after_storage_tick(WatchAppSleepRequestState *sleep_request, uint32_t *last_storage_commit_ms)
{
    uint32_t commit_ms;

    commit_ms = storage_service_get_last_commit_ms();
    if (last_storage_commit_ms != NULL && commit_ms != *last_storage_commit_ms) {
        *last_storage_commit_ms = commit_ms;
        sensor_service_note_storage_commit_result(storage_service_get_last_commit_reason(),
                                                  storage_service_get_last_commit_ok());
        if (sleep_request != NULL && sleep_request->pending && !storage_service_get_last_commit_ok()) {
            sleep_request->pending = false;
            sleep_request->reason = SLEEP_REASON_NONE;
            sleep_request->requested_at_ms = 0U;
            ui_core_mark_dirty();
        }
    }

    if (sleep_request == NULL || !sleep_request->pending) {
        return;
    }

    UiRuntimeSnapshot ui_runtime;

    if (sleep_request->reason == SLEEP_REASON_AUTO_TIMEOUT &&
        ui_get_runtime_snapshot(&ui_runtime) &&
        ui_runtime.last_input_ms > sleep_request->requested_at_ms) {
        sleep_request->pending = false;
        sleep_request->reason = SLEEP_REASON_NONE;
        sleep_request->requested_at_ms = 0U;
        return;
    }

    if (storage_service_has_pending() || storage_service_is_transaction_active()) {
        return;
    }

    watch_app_sleep_now(sleep_request->reason, sleep_request);
}
