#ifndef APP_COMMAND_H
#define APP_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
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
    APP_COMMAND_RESET_APP_STATE,
    APP_COMMAND_FACTORY_RESET,
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

typedef enum {
    APP_COMMAND_RESULT_OK = 0,
    APP_COMMAND_RESULT_INVALID_DESCRIPTOR,
    APP_COMMAND_RESULT_INVALID_SOURCE,
    APP_COMMAND_RESULT_OUT_OF_RANGE,
    APP_COMMAND_RESULT_BLOCKED,
    APP_COMMAND_RESULT_BACKEND_FAILURE,
    APP_COMMAND_RESULT_UNSUPPORTED
} AppCommandResultCode;

typedef struct {
    AppCommandType type;
    const char *wire_name;
    bool web_exposed;
} AppCommandDescriptor;

typedef struct {
    bool ok;
    AppCommandResultCode code;
} AppCommandExecutionResult;

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
 * @brief Return the immutable descriptor for a command type.
 *
 * @param[in] type Command type to describe.
 * @return Descriptor pointer when the command exists; NULL when the type is unsupported.
 * @throws None.
 */
const AppCommandDescriptor *app_command_describe(AppCommandType type);

/**
 * @brief Look up a command descriptor by its stable wire name.
 *
 * @param[in] wire_name Stable external command name.
 * @return Descriptor pointer when a matching command exists; NULL otherwise.
 * @throws None.
 */
const AppCommandDescriptor *app_command_describe_by_name(const char *wire_name);

/**
 * @brief Resolve a companion SET key into a concrete command type.
 *
 * @param[in] companion_key Stable companion protocol key.
 * @param[out] out_type Destination command type.
 * @return true when the key maps to a supported command; false otherwise.
 * @throws None.
 */
bool app_command_type_from_companion_key(const char *companion_key, AppCommandType *out_type);

/**
 * @brief Report the number of descriptors in the immutable command catalog.
 *
 * @param None.
 * @return Catalog entry count.
 * @throws None.
 */
size_t app_command_catalog_count(void);

/**
 * @brief Return the descriptor at the requested catalog index.
 *
 * @param[in] index Zero-based catalog index.
 * @return Descriptor pointer when index is valid; NULL otherwise.
 * @throws None.
 */
const AppCommandDescriptor *app_command_catalog_at(size_t index);

/**
 * @brief Return a stable printable name for a command execution result code.
 *
 * @param[in] code Result code to stringify.
 * @return Stable result code string.
 * @throws None.
 */
const char *app_command_result_code_name(AppCommandResultCode code);

/**
 * @brief Execute a single user- or service-originated runtime command through the shared mutation ingress.
 *
 * This detailed entry point records whether the command was applied, blocked, rejected,
 * or failed due to a backend/runtime error.
 *
 * @param[in] command Command descriptor to execute. Must not be NULL and must carry a supported source/type pair.
 * @param[in,out] last_sensor_sensitivity Optional mirror updated when the command changes sensor sensitivity.
 * @param[out] out_result Optional detailed result destination.
 * @return true when the command executed or was intentionally accepted as a no-op; false when the descriptor is invalid or the command was blocked.
 * @throws None.
 * @boundary_behavior Returns false for NULL commands, unknown command sources, unknown command types, blocked safe-mode clear requests, failed sensor reinitialization requests, or invalid indexed alarm mutations.
 */
bool app_command_execute_detailed(const AppCommand *command,
                                  uint8_t *last_sensor_sensitivity,
                                  AppCommandExecutionResult *out_result);

/**
 * @brief Execute a command using the legacy boolean-only result contract.
 *
 * @param[in] command Command descriptor to execute.
 * @param[in,out] last_sensor_sensitivity Optional mirror updated when the command changes sensor sensitivity.
 * @return true when the command executed successfully; false otherwise.
 * @throws None.
 */
bool app_command_execute(const AppCommand *command, uint8_t *last_sensor_sensitivity);

#ifdef __cplusplus
}
#endif

#endif
