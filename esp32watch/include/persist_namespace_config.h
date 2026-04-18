#ifndef PERSIST_NAMESPACE_CONFIG_H
#define PERSIST_NAMESPACE_CONFIG_H

/*
 * Preferences namespace ownership map.
 *
 * Device configuration and user-managed settings remain durable across reboots.
 * Time recovery state is durable so the runtime can recover a coarse epoch when
 * network sync is unavailable.
 * Game statistics remain durable user data.
 *
 * ESP32-S3 watch targets additionally persist settings, alarms, and sensor
 * calibration through dedicated app-state namespaces so the runtime can survive
 * full power loss while still mirroring the latest snapshot into reset-domain
 * RTC registers for compatibility with legacy code paths and diagnostics.
 */
#define APP_PREFS_NS_DEVICE_CONFIG       "watch_cfg"
#define APP_PREFS_NS_TIME_RECOVERY       "watch_rtc"
#define APP_PREFS_NS_GAME_STATS          "watch_game"
#define APP_PREFS_NS_APP_SETTINGS        "watch_set"
#define APP_PREFS_NS_APP_ALARMS          "watch_alarm"
#define APP_PREFS_NS_SENSOR_CALIBRATION  "watch_cal"

#endif
