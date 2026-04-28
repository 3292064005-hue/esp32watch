#include "app_command_registry.h"
#include "board_features.h"
#include "app_limits.h"

#define CMD_NONE(type, wire, web, cap, destructive) \
    {type, wire, web, APP_COMMAND_PAYLOAD_NONE, NULL, 0, 0, cap, destructive}
#define CMD_U8(type, wire, web, min_v, max_v, cap, destructive) \
    {type, wire, web, APP_COMMAND_PAYLOAD_U8_VALUE, "value", min_v, max_v, cap, destructive}
#define CMD_U32(type, wire, web, min_v, max_v, cap, destructive) \
    {type, wire, web, APP_COMMAND_PAYLOAD_U32_VALUE, "value", min_v, max_v, cap, destructive}
#define CMD_I8(type, wire, web, min_v, max_v, cap, destructive) \
    {type, wire, web, APP_COMMAND_PAYLOAD_I8_DELTA, "delta", min_v, max_v, cap, destructive}
#define CMD_I32(type, wire, web, min_v, max_v, cap, destructive) \
    {type, wire, web, APP_COMMAND_PAYLOAD_I32_DELTA, "delta", min_v, max_v, cap, destructive}
#define CMD_BOOL(type, wire, web, cap, destructive) \
    {type, wire, web, APP_COMMAND_PAYLOAD_BOOL_ENABLED, "enabled", 0, 1, cap, destructive}
#define CMD_CUSTOM(type, wire, web, kind, field, cap, destructive) \
    {type, wire, web, kind, field, 0, 0, cap, destructive}

static const AppCommandDescriptor kCoreCommandCatalog[] = {
    CMD_U8(APP_COMMAND_SET_BRIGHTNESS, "setBrightness", true, 0, 3, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_U32(APP_COMMAND_SET_GOAL, "setGoal", false, 1, 100000, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_U8(APP_COMMAND_SET_WATCHFACE, "setWatchface", true, 0, 2, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_I8(APP_COMMAND_CYCLE_WATCHFACE, "cycleWatchface", false, -1, 1, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_U8(APP_COMMAND_SET_SCREEN_TIMEOUT_IDX, "setScreenTimeoutIndex", false, 0, 2, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_I8(APP_COMMAND_CYCLE_SCREEN_TIMEOUT, "cycleScreenTimeout", false, -1, 1, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_U8(APP_COMMAND_SET_SENSOR_SENSITIVITY, "setSensorSensitivity", false, 0, 2, APP_COMMAND_CAPABILITY_SENSOR, false),
    CMD_BOOL(APP_COMMAND_SET_AUTO_WAKE, "setAutoWake", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_BOOL(APP_COMMAND_SET_AUTO_SLEEP, "setAutoSleep", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_BOOL(APP_COMMAND_SET_DND, "setDnd", false, APP_COMMAND_CAPABILITY_NONE, false),
#if APP_FEATURE_VIBRATION
    CMD_BOOL(APP_COMMAND_SET_VIBRATE, "setVibrate", false, APP_COMMAND_CAPABILITY_VIBRATION, false),
#endif
    CMD_BOOL(APP_COMMAND_SET_SHOW_SECONDS, "setShowSeconds", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_BOOL(APP_COMMAND_SET_ANIMATIONS, "setAnimations", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_NONE(APP_COMMAND_RESET_APP_STATE, "resetAppState", false, APP_COMMAND_CAPABILITY_RESET, true),
    CMD_NONE(APP_COMMAND_FACTORY_RESET, "factoryReset", false, APP_COMMAND_CAPABILITY_RESET, true),
    CMD_NONE(APP_COMMAND_RESTORE_DEFAULTS, "restoreDefaults", false, APP_COMMAND_CAPABILITY_RESET, true),
    CMD_I8(APP_COMMAND_SELECT_ALARM_OFFSET, "selectAlarmOffset", false, -1, 1, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_CUSTOM(APP_COMMAND_SET_ALARM_ENABLED_AT, "setAlarmEnabledAt", false, APP_COMMAND_PAYLOAD_ALARM_ENABLED, "alarm", APP_COMMAND_CAPABILITY_NONE, false),
    CMD_CUSTOM(APP_COMMAND_SET_ALARM_TIME_AT, "setAlarmTimeAt", false, APP_COMMAND_PAYLOAD_ALARM_TIME, "alarm", APP_COMMAND_CAPABILITY_NONE, false),
    CMD_CUSTOM(APP_COMMAND_SET_ALARM_REPEAT_MASK_AT, "setAlarmRepeatMaskAt", false, APP_COMMAND_PAYLOAD_ALARM_REPEAT, "alarm", APP_COMMAND_CAPABILITY_NONE, false),
    CMD_NONE(APP_COMMAND_STOPWATCH_TOGGLE, "stopwatchToggle", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_NONE(APP_COMMAND_STOPWATCH_RESET, "stopwatchReset", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_NONE(APP_COMMAND_STOPWATCH_LAP, "stopwatchLap", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_NONE(APP_COMMAND_TIMER_TOGGLE, "timerToggle", false, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_I32(APP_COMMAND_TIMER_ADJUST_SECONDS, "timerAdjustSeconds", false, -3600, 3600, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_I8(APP_COMMAND_TIMER_CYCLE_PRESET, "timerCyclePreset", false, -1, 1, APP_COMMAND_CAPABILITY_NONE, false),
    CMD_CUSTOM(APP_COMMAND_SET_DATETIME, "setDatetime", false, APP_COMMAND_PAYLOAD_DATETIME, "dateTime", APP_COMMAND_CAPABILITY_NONE, false),
    CMD_CUSTOM(APP_COMMAND_SET_GAME_HIGH_SCORE, "setGameHighScore", false, APP_COMMAND_PAYLOAD_GAME_HIGH_SCORE, "gameScore", APP_COMMAND_CAPABILITY_NONE, false),
    CMD_NONE(APP_COMMAND_SENSOR_REINIT, "sensorReinit", true, APP_COMMAND_CAPABILITY_SENSOR, false),
    CMD_NONE(APP_COMMAND_SENSOR_CALIBRATION, "sensorCalibration", true, APP_COMMAND_CAPABILITY_SENSOR, false),
    CMD_NONE(APP_COMMAND_STORAGE_MANUAL_FLUSH, "storageManualFlush", true, APP_COMMAND_CAPABILITY_STORAGE, false),
    CMD_NONE(APP_COMMAND_CLEAR_SAFE_MODE, "clearSafeMode", true, APP_COMMAND_CAPABILITY_SAFE_MODE, false)
};

void app_command_register_core_module(void)
{
    (void)app_command_registry_register_block("core", kCoreCommandCatalog, sizeof(kCoreCommandCatalog) / sizeof(kCoreCommandCatalog[0]));
    (void)app_command_registry_register_companion_binding("BRIGHTNESS", APP_COMMAND_SET_BRIGHTNESS);
    (void)app_command_registry_register_companion_binding("GOAL", APP_COMMAND_SET_GOAL);
    (void)app_command_registry_register_companion_binding("WATCHFACE", APP_COMMAND_SET_WATCHFACE);
    (void)app_command_registry_register_companion_binding("SENSOR_SENS", APP_COMMAND_SET_SENSOR_SENSITIVITY);
    (void)app_command_registry_register_companion_binding("SCREEN_TIMEOUT", APP_COMMAND_SET_SCREEN_TIMEOUT_IDX);
    (void)app_command_registry_register_companion_binding("AUTO_WAKE", APP_COMMAND_SET_AUTO_WAKE);
    (void)app_command_registry_register_companion_binding("AUTO_SLEEP", APP_COMMAND_SET_AUTO_SLEEP);
    (void)app_command_registry_register_companion_binding("DND", APP_COMMAND_SET_DND);
#if APP_FEATURE_VIBRATION
    (void)app_command_registry_register_companion_binding("VIBRATE", APP_COMMAND_SET_VIBRATE);
#endif
    (void)app_command_registry_register_companion_binding("SHOW_SECONDS", APP_COMMAND_SET_SHOW_SECONDS);
    (void)app_command_registry_register_companion_binding("ANIMATIONS", APP_COMMAND_SET_ANIMATIONS);
}
