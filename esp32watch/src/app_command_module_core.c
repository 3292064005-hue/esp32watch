#include "app_command_registry.h"

static const AppCommandDescriptor kCoreCommandCatalog[] = {
    {APP_COMMAND_SET_BRIGHTNESS, "setBrightness", true},
    {APP_COMMAND_SET_GOAL, "setGoal", false},
    {APP_COMMAND_SET_WATCHFACE, "setWatchface", true},
    {APP_COMMAND_CYCLE_WATCHFACE, "cycleWatchface", false},
    {APP_COMMAND_SET_SCREEN_TIMEOUT_IDX, "setScreenTimeoutIndex", false},
    {APP_COMMAND_CYCLE_SCREEN_TIMEOUT, "cycleScreenTimeout", false},
    {APP_COMMAND_SET_SENSOR_SENSITIVITY, "setSensorSensitivity", false},
    {APP_COMMAND_SET_AUTO_WAKE, "setAutoWake", false},
    {APP_COMMAND_SET_AUTO_SLEEP, "setAutoSleep", false},
    {APP_COMMAND_SET_DND, "setDnd", false},
#if APP_FEATURE_VIBRATION
    {APP_COMMAND_SET_VIBRATE, "setVibrate", false},
#endif
    {APP_COMMAND_SET_SHOW_SECONDS, "setShowSeconds", false},
    {APP_COMMAND_SET_ANIMATIONS, "setAnimations", false},
    {APP_COMMAND_RESET_APP_STATE, "resetAppState", false},
    {APP_COMMAND_FACTORY_RESET, "factoryReset", false},
    {APP_COMMAND_RESTORE_DEFAULTS, "restoreDefaults", false},
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

