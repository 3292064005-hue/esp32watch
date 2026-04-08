#ifndef APP_COMMAND_H
#define APP_COMMAND_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_COMMAND_SOURCE_UI = 0,
    APP_COMMAND_SOURCE_COMPANION,
    APP_COMMAND_SOURCE_SERVICE,
    APP_COMMAND_SOURCE_RECOVERY
} AppCommandSource;

typedef enum {
    APP_COMMAND_NONE = 0,
    APP_COMMAND_SET_BRIGHTNESS,
    APP_COMMAND_SET_GOAL,
    APP_COMMAND_SET_WATCHFACE,
    APP_COMMAND_CYCLE_WATCHFACE,
    APP_COMMAND_SET_SCREEN_TIMEOUT_IDX,
    APP_COMMAND_CYCLE_SCREEN_TIMEOUT,
    APP_COMMAND_SET_SENSOR_SENSITIVITY,
    APP_COMMAND_SET_AUTO_WAKE,
    APP_COMMAND_SET_AUTO_SLEEP,
    APP_COMMAND_SET_DND,
    APP_COMMAND_SET_VIBRATE,
    APP_COMMAND_SET_SHOW_SECONDS,
    APP_COMMAND_SET_ANIMATIONS,
    APP_COMMAND_RESTORE_DEFAULTS,
    APP_COMMAND_SELECT_ALARM_OFFSET,
    APP_COMMAND_SET_ALARM_ENABLED_AT,
    APP_COMMAND_SET_ALARM_TIME_AT,
    APP_COMMAND_SET_ALARM_REPEAT_MASK_AT,
    APP_COMMAND_STOPWATCH_TOGGLE,
    APP_COMMAND_STOPWATCH_RESET,
    APP_COMMAND_STOPWATCH_LAP,
    APP_COMMAND_TIMER_TOGGLE,
    APP_COMMAND_TIMER_ADJUST_SECONDS,
    APP_COMMAND_TIMER_CYCLE_PRESET,
    APP_COMMAND_SET_DATETIME,
    APP_COMMAND_SET_GAME_HIGH_SCORE,
    APP_COMMAND_SENSOR_REINIT,
    APP_COMMAND_SENSOR_CALIBRATION,
    APP_COMMAND_STORAGE_MANUAL_FLUSH,
    APP_COMMAND_CLEAR_SAFE_MODE
} AppCommandType;

typedef struct {
    AppCommandSource source;
    AppCommandType type;
    union {
        uint8_t u8;
        uint32_t u32;
        int8_t delta_i8;
        int32_t delta_i32;
        bool enabled;
        uint32_t now_ms;
        DateTime date_time;
        struct {
            uint8_t index;
            bool enabled;
        } alarm_enabled;
        struct {
            uint8_t index;
            uint8_t hour;
            uint8_t minute;
        } alarm_time;
        struct {
            uint8_t index;
            uint8_t repeat_mask;
        } alarm_repeat;
        struct {
            uint8_t game_id;
            uint16_t score;
        } game_high_score;
    } data;
} AppCommand;

/**
 * @brief Execute a single user- or service-originated runtime command through the shared mutation ingress.
 *
 * @param[in] command Command descriptor to execute. Must not be NULL and must carry a supported source/type pair.
 * @param[in,out] last_sensor_sensitivity Optional mirror updated when the command changes sensor sensitivity.
 * @return true when the command executed or was intentionally accepted as a no-op; false when the descriptor is invalid or the command was blocked.
 * @throws None.
 * @boundary_behavior Returns false for NULL commands, unknown command sources, unknown command types, blocked safe-mode clear requests, or failed sensor reinitialization requests.
 */
bool app_command_execute(const AppCommand *command, uint8_t *last_sensor_sensitivity);

#ifdef __cplusplus
}
#endif

#endif
