#include "app_command.h"
#include "app_limits.h"
#include "services/diag_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include <stddef.h>

static bool app_command_source_valid(AppCommandSource source)
{
    return source == APP_COMMAND_SOURCE_UI ||
           source == APP_COMMAND_SOURCE_COMPANION ||
           source == APP_COMMAND_SOURCE_SERVICE ||
           source == APP_COMMAND_SOURCE_RECOVERY;
}

bool app_command_execute(const AppCommand *command, uint8_t *last_sensor_sensitivity)
{
    if (command == NULL || !app_command_source_valid(command->source)) {
        return false;
    }

    switch (command->type) {
        case APP_COMMAND_SET_BRIGHTNESS:
            model_set_brightness(command->data.u8);
            return true;
        case APP_COMMAND_SET_GOAL:
            model_set_goal(command->data.u32);
            return true;
        case APP_COMMAND_SET_WATCHFACE:
            model_set_watchface(command->data.u8);
            return true;
        case APP_COMMAND_CYCLE_WATCHFACE:
            model_cycle_watchface(command->data.delta_i8);
            return true;
        case APP_COMMAND_SET_SCREEN_TIMEOUT_IDX:
            model_set_screen_timeout_idx(command->data.u8);
            return true;
        case APP_COMMAND_CYCLE_SCREEN_TIMEOUT:
            model_cycle_screen_timeout(command->data.delta_i8);
            return true;
        case APP_COMMAND_SET_SENSOR_SENSITIVITY:
            model_set_sensor_sensitivity(command->data.u8);
            if (last_sensor_sensitivity != NULL) {
                *last_sensor_sensitivity = command->data.u8;
            }
            return true;
        case APP_COMMAND_SET_AUTO_WAKE:
            model_set_auto_wake(command->data.enabled);
            return true;
        case APP_COMMAND_SET_AUTO_SLEEP:
            model_set_auto_sleep(command->data.enabled);
            return true;
        case APP_COMMAND_SET_DND:
            model_set_dnd(command->data.enabled);
            return true;
        case APP_COMMAND_SET_VIBRATE:
            model_set_vibrate(command->data.enabled);
            return true;
        case APP_COMMAND_SET_SHOW_SECONDS:
            model_set_show_seconds(command->data.enabled);
            return true;
        case APP_COMMAND_SET_ANIMATIONS:
            model_set_animations(command->data.enabled);
            return true;
        case APP_COMMAND_RESTORE_DEFAULTS:
            model_restore_defaults();
            return true;
        case APP_COMMAND_SELECT_ALARM_OFFSET:
            model_select_alarm_offset(command->data.delta_i8);
            return true;
        case APP_COMMAND_SET_ALARM_ENABLED_AT:
            if (command->data.alarm_enabled.index >= APP_MAX_ALARMS) {
                return false;
            }
            model_set_alarm_enabled_at(command->data.alarm_enabled.index,
                                       command->data.alarm_enabled.enabled);
            return true;
        case APP_COMMAND_SET_ALARM_TIME_AT:
            if (command->data.alarm_time.index >= APP_MAX_ALARMS) {
                return false;
            }
            model_set_alarm_time_at(command->data.alarm_time.index,
                                    command->data.alarm_time.hour,
                                    command->data.alarm_time.minute);
            return true;
        case APP_COMMAND_SET_ALARM_REPEAT_MASK_AT:
            if (command->data.alarm_repeat.index >= APP_MAX_ALARMS) {
                return false;
            }
            model_set_alarm_repeat_mask_at(command->data.alarm_repeat.index,
                                           command->data.alarm_repeat.repeat_mask);
            return true;
        case APP_COMMAND_STOPWATCH_TOGGLE:
            model_stopwatch_toggle(command->data.now_ms);
            return true;
        case APP_COMMAND_STOPWATCH_RESET:
            model_stopwatch_reset();
            return true;
        case APP_COMMAND_STOPWATCH_LAP:
            model_stopwatch_lap();
            return true;
        case APP_COMMAND_TIMER_TOGGLE:
            model_timer_toggle(command->data.now_ms);
            return true;
        case APP_COMMAND_TIMER_ADJUST_SECONDS:
            model_timer_adjust_seconds(command->data.delta_i32);
            return true;
        case APP_COMMAND_TIMER_CYCLE_PRESET:
            model_timer_cycle_preset(command->data.delta_i8);
            return true;
        case APP_COMMAND_SET_DATETIME:
            model_set_datetime(&command->data.date_time);
            return true;
        case APP_COMMAND_SENSOR_REINIT:
            return sensor_service_force_reinit();
        case APP_COMMAND_SENSOR_CALIBRATION:
            sensor_service_request_calibration();
            return true;
        case APP_COMMAND_STORAGE_MANUAL_FLUSH:
            storage_service_request_commit(STORAGE_COMMIT_REASON_MANUAL);
            return true;
        case APP_COMMAND_CLEAR_SAFE_MODE:
            if (!diag_service_can_clear_safe_mode()) {
                return false;
            }
            diag_service_clear_safe_mode();
            return true;
        case APP_COMMAND_NONE:
        default:
            return false;
    }
}
