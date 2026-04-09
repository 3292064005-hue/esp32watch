#include "app_command.h"
#include "app_limits.h"
#include "services/diag_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include <stddef.h>
#include <string.h>

static const AppCommandDescriptor kAppCommandCatalog[] = {
    {APP_COMMAND_SET_BRIGHTNESS, "setBrightness", false},
    {APP_COMMAND_SET_GOAL, "setGoal", false},
    {APP_COMMAND_SET_WATCHFACE, "setWatchface", false},
    {APP_COMMAND_CYCLE_WATCHFACE, "cycleWatchface", false},
    {APP_COMMAND_SET_SCREEN_TIMEOUT_IDX, "setScreenTimeoutIndex", false},
    {APP_COMMAND_CYCLE_SCREEN_TIMEOUT, "cycleScreenTimeout", false},
    {APP_COMMAND_SET_SENSOR_SENSITIVITY, "setSensorSensitivity", false},
    {APP_COMMAND_SET_AUTO_WAKE, "setAutoWake", false},
    {APP_COMMAND_SET_AUTO_SLEEP, "setAutoSleep", false},
    {APP_COMMAND_SET_DND, "setDnd", false},
    {APP_COMMAND_SET_VIBRATE, "setVibrate", false},
    {APP_COMMAND_SET_SHOW_SECONDS, "setShowSeconds", false},
    {APP_COMMAND_SET_ANIMATIONS, "setAnimations", false},
    {APP_COMMAND_RESTORE_DEFAULTS, "restoreDefaults", true},
    {APP_COMMAND_SELECT_ALARM_OFFSET, "selectAlarmOffset", false},
    {APP_COMMAND_SET_ALARM_ENABLED_AT, "setAlarmEnabledAt", false},
    {APP_COMMAND_SET_ALARM_TIME_AT, "setAlarmTimeAt", false},
    {APP_COMMAND_SET_ALARM_REPEAT_MASK_AT, "setAlarmRepeatMaskAt", false},
    {APP_COMMAND_STOPWATCH_TOGGLE, "stopwatchToggle", false},
    {APP_COMMAND_STOPWATCH_RESET, "stopwatchReset", false},
    {APP_COMMAND_STOPWATCH_LAP, "stopwatchLap", false},
    {APP_COMMAND_TIMER_TOGGLE, "timerToggle", false},
    {APP_COMMAND_TIMER_ADJUST_SECONDS, "timerAdjustSeconds", false},
    {APP_COMMAND_TIMER_CYCLE_PRESET, "timerCyclePreset", false},
    {APP_COMMAND_SET_DATETIME, "setDatetime", false},
    {APP_COMMAND_SET_GAME_HIGH_SCORE, "setGameHighScore", false},
    {APP_COMMAND_SENSOR_REINIT, "sensorReinit", true},
    {APP_COMMAND_SENSOR_CALIBRATION, "sensorCalibration", true},
    {APP_COMMAND_STORAGE_MANUAL_FLUSH, "storageManualFlush", true},
    {APP_COMMAND_CLEAR_SAFE_MODE, "clearSafeMode", true}
};

static bool app_command_source_valid(AppCommandSource source)
{
    return source == APP_COMMAND_SOURCE_UI ||
           source == APP_COMMAND_SOURCE_COMPANION ||
           source == APP_COMMAND_SOURCE_SERVICE ||
           source == APP_COMMAND_SOURCE_RECOVERY;
}

static void app_command_set_result(AppCommandExecutionResult *out_result, bool ok, AppCommandResultCode code)
{
    if (out_result == NULL) {
        return;
    }
    out_result->ok = ok;
    out_result->code = code;
}

const AppCommandDescriptor *app_command_describe(AppCommandType type)
{
    size_t i;
    for (i = 0; i < (sizeof(kAppCommandCatalog) / sizeof(kAppCommandCatalog[0])); ++i) {
        if (kAppCommandCatalog[i].type == type) {
            return &kAppCommandCatalog[i];
        }
    }
    return NULL;
}

const AppCommandDescriptor *app_command_describe_by_name(const char *wire_name)
{
    size_t i;
    if (wire_name == NULL || wire_name[0] == '\0') {
        return NULL;
    }
    for (i = 0; i < (sizeof(kAppCommandCatalog) / sizeof(kAppCommandCatalog[0])); ++i) {
        if (strcmp(kAppCommandCatalog[i].wire_name, wire_name) == 0) {
            return &kAppCommandCatalog[i];
        }
    }
    return NULL;
}

bool app_command_type_from_companion_key(const char *companion_key, AppCommandType *out_type)
{
    if (out_type == NULL || companion_key == NULL || companion_key[0] == '\0') {
        return false;
    }
    if (strcmp(companion_key, "BRIGHTNESS") == 0) {
        *out_type = APP_COMMAND_SET_BRIGHTNESS;
        return true;
    }
    if (strcmp(companion_key, "GOAL") == 0) {
        *out_type = APP_COMMAND_SET_GOAL;
        return true;
    }
    if (strcmp(companion_key, "WATCHFACE") == 0) {
        *out_type = APP_COMMAND_SET_WATCHFACE;
        return true;
    }
    if (strcmp(companion_key, "SENSOR_SENS") == 0) {
        *out_type = APP_COMMAND_SET_SENSOR_SENSITIVITY;
        return true;
    }
    if (strcmp(companion_key, "SCREEN_TIMEOUT") == 0) {
        *out_type = APP_COMMAND_SET_SCREEN_TIMEOUT_IDX;
        return true;
    }
    if (strcmp(companion_key, "AUTO_WAKE") == 0) {
        *out_type = APP_COMMAND_SET_AUTO_WAKE;
        return true;
    }
    if (strcmp(companion_key, "AUTO_SLEEP") == 0) {
        *out_type = APP_COMMAND_SET_AUTO_SLEEP;
        return true;
    }
    if (strcmp(companion_key, "DND") == 0) {
        *out_type = APP_COMMAND_SET_DND;
        return true;
    }
    if (strcmp(companion_key, "VIBRATE") == 0) {
        *out_type = APP_COMMAND_SET_VIBRATE;
        return true;
    }
    if (strcmp(companion_key, "SHOW_SECONDS") == 0) {
        *out_type = APP_COMMAND_SET_SHOW_SECONDS;
        return true;
    }
    if (strcmp(companion_key, "ANIMATIONS") == 0) {
        *out_type = APP_COMMAND_SET_ANIMATIONS;
        return true;
    }
    return false;
}

size_t app_command_catalog_count(void)
{
    return sizeof(kAppCommandCatalog) / sizeof(kAppCommandCatalog[0]);
}

const AppCommandDescriptor *app_command_catalog_at(size_t index)
{
    if (index >= app_command_catalog_count()) {
        return NULL;
    }
    return &kAppCommandCatalog[index];
}

const char *app_command_result_code_name(AppCommandResultCode code)
{
    switch (code) {
        case APP_COMMAND_RESULT_OK: return "OK";
        case APP_COMMAND_RESULT_INVALID_DESCRIPTOR: return "INVALID_DESCRIPTOR";
        case APP_COMMAND_RESULT_INVALID_SOURCE: return "INVALID_SOURCE";
        case APP_COMMAND_RESULT_OUT_OF_RANGE: return "OUT_OF_RANGE";
        case APP_COMMAND_RESULT_BLOCKED: return "BLOCKED";
        case APP_COMMAND_RESULT_BACKEND_FAILURE: return "BACKEND_FAILURE";
        case APP_COMMAND_RESULT_UNSUPPORTED:
        default:
            return "UNSUPPORTED";
    }
}

bool app_command_execute_detailed(const AppCommand *command,
                                  uint8_t *last_sensor_sensitivity,
                                  AppCommandExecutionResult *out_result)
{
    if (command == NULL) {
        app_command_set_result(out_result, false, APP_COMMAND_RESULT_INVALID_DESCRIPTOR);
        return false;
    }
    if (!app_command_source_valid(command->source)) {
        app_command_set_result(out_result, false, APP_COMMAND_RESULT_INVALID_SOURCE);
        return false;
    }
    if (app_command_describe(command->type) == NULL) {
        app_command_set_result(out_result, false, APP_COMMAND_RESULT_UNSUPPORTED);
        return false;
    }

    switch (command->type) {
        case APP_COMMAND_SET_BRIGHTNESS:
            model_set_brightness(command->data.u8);
            break;
        case APP_COMMAND_SET_GOAL:
            model_set_goal(command->data.u32);
            break;
        case APP_COMMAND_SET_WATCHFACE:
            model_set_watchface(command->data.u8);
            break;
        case APP_COMMAND_CYCLE_WATCHFACE:
            model_cycle_watchface(command->data.delta_i8);
            break;
        case APP_COMMAND_SET_SCREEN_TIMEOUT_IDX:
            model_set_screen_timeout_idx(command->data.u8);
            break;
        case APP_COMMAND_CYCLE_SCREEN_TIMEOUT:
            model_cycle_screen_timeout(command->data.delta_i8);
            break;
        case APP_COMMAND_SET_SENSOR_SENSITIVITY:
            model_set_sensor_sensitivity(command->data.u8);
            if (last_sensor_sensitivity != NULL) {
                *last_sensor_sensitivity = command->data.u8;
            }
            break;
        case APP_COMMAND_SET_AUTO_WAKE:
            model_set_auto_wake(command->data.enabled);
            break;
        case APP_COMMAND_SET_AUTO_SLEEP:
            model_set_auto_sleep(command->data.enabled);
            break;
        case APP_COMMAND_SET_DND:
            model_set_dnd(command->data.enabled);
            break;
        case APP_COMMAND_SET_VIBRATE:
            model_set_vibrate(command->data.enabled);
            break;
        case APP_COMMAND_SET_SHOW_SECONDS:
            model_set_show_seconds(command->data.enabled);
            break;
        case APP_COMMAND_SET_ANIMATIONS:
            model_set_animations(command->data.enabled);
            break;
        case APP_COMMAND_RESTORE_DEFAULTS:
            model_restore_defaults();
            break;
        case APP_COMMAND_SELECT_ALARM_OFFSET:
            model_select_alarm_offset(command->data.delta_i8);
            break;
        case APP_COMMAND_SET_ALARM_ENABLED_AT:
            if (command->data.alarm_enabled.index >= APP_MAX_ALARMS) {
                app_command_set_result(out_result, false, APP_COMMAND_RESULT_OUT_OF_RANGE);
                return false;
            }
            model_set_alarm_enabled_at(command->data.alarm_enabled.index,
                                       command->data.alarm_enabled.enabled);
            break;
        case APP_COMMAND_SET_ALARM_TIME_AT:
            if (command->data.alarm_time.index >= APP_MAX_ALARMS) {
                app_command_set_result(out_result, false, APP_COMMAND_RESULT_OUT_OF_RANGE);
                return false;
            }
            model_set_alarm_time_at(command->data.alarm_time.index,
                                    command->data.alarm_time.hour,
                                    command->data.alarm_time.minute);
            break;
        case APP_COMMAND_SET_ALARM_REPEAT_MASK_AT:
            if (command->data.alarm_repeat.index >= APP_MAX_ALARMS) {
                app_command_set_result(out_result, false, APP_COMMAND_RESULT_OUT_OF_RANGE);
                return false;
            }
            model_set_alarm_repeat_mask_at(command->data.alarm_repeat.index,
                                           command->data.alarm_repeat.repeat_mask);
            break;
        case APP_COMMAND_STOPWATCH_TOGGLE:
            model_stopwatch_toggle(command->data.now_ms);
            break;
        case APP_COMMAND_STOPWATCH_RESET:
            model_stopwatch_reset();
            break;
        case APP_COMMAND_STOPWATCH_LAP:
            model_stopwatch_lap();
            break;
        case APP_COMMAND_TIMER_TOGGLE:
            model_timer_toggle(command->data.now_ms);
            break;
        case APP_COMMAND_TIMER_ADJUST_SECONDS:
            model_timer_adjust_seconds(command->data.delta_i32);
            break;
        case APP_COMMAND_TIMER_CYCLE_PRESET:
            model_timer_cycle_preset(command->data.delta_i8);
            break;
        case APP_COMMAND_SET_DATETIME:
            model_set_datetime(&command->data.date_time);
            break;
        case APP_COMMAND_SET_GAME_HIGH_SCORE:
            if (!model_set_game_high_score((GameId)command->data.game_high_score.game_id,
                                           command->data.game_high_score.score)) {
                app_command_set_result(out_result, false, APP_COMMAND_RESULT_OUT_OF_RANGE);
                return false;
            }
            break;
        case APP_COMMAND_SENSOR_REINIT:
            if (!sensor_service_force_reinit()) {
                app_command_set_result(out_result, false, APP_COMMAND_RESULT_BACKEND_FAILURE);
                return false;
            }
            break;
        case APP_COMMAND_SENSOR_CALIBRATION:
            sensor_service_request_calibration();
            break;
        case APP_COMMAND_STORAGE_MANUAL_FLUSH:
            storage_service_request_commit(STORAGE_COMMIT_REASON_MANUAL);
            break;
        case APP_COMMAND_CLEAR_SAFE_MODE:
            if (!diag_service_can_clear_safe_mode()) {
                app_command_set_result(out_result, false, APP_COMMAND_RESULT_BLOCKED);
                return false;
            }
            diag_service_clear_safe_mode();
            break;
        case APP_COMMAND_NONE:
        default:
            app_command_set_result(out_result, false, APP_COMMAND_RESULT_UNSUPPORTED);
            return false;
    }

    app_command_set_result(out_result, true, APP_COMMAND_RESULT_OK);
    return true;
}

bool app_command_execute(const AppCommand *command, uint8_t *last_sensor_sensitivity)
{
    return app_command_execute_detailed(command, last_sensor_sensitivity, NULL);
}
