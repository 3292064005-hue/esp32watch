#include <Preferences.h>
#include <string.h>

extern "C" {
#include "persist.h"
#include "main.h"
#include "app_config.h"
#include "board_features.h"
#include "common/storage_codec.h"
#include "persist_namespace_config.h"
}
#include "persist_preferences.h"
#include "persist_codec.h"
#include "persist_internal.hpp"

void read_raw(uint16_t *settings0, uint16_t *settings1, uint16_t *settings2,
              uint16_t *alarm0, uint16_t *alarm1, uint16_t *alarm2, uint16_t *alarm_meta,
              uint16_t *cal0, uint16_t *cal1, uint16_t *cal2, uint16_t *cal3)
{
    *settings0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS0);
    *settings1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS1);
    *settings2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS2);
    *alarm0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM);
    *alarm1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM1);
    *alarm2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM2);
    *alarm_meta = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META);
    *cal0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL0);
    *cal1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL1);
    *cal2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL2);
    *cal3 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL3);
}

void write_crc(void)
{
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_CRC, persist_get_calculated_crc());
}

bool durable_app_state_enabled(void)
{
#if APP_STORAGE_USE_PREFERENCES_APP_STATE
    return true;
#else
    return false;
#endif
}

bool persist_game_stats_open(bool read_only)
{
    return persist_preferences_begin(g_game_stats_prefs, PERSIST_PREFS_DOMAIN_GAME_STATS, read_only);
}

static bool persist_settings_open(bool read_only)
{
    return persist_preferences_begin(g_settings_prefs, PERSIST_PREFS_DOMAIN_APP_SETTINGS, read_only);
}

static bool persist_alarms_open(bool read_only)
{
    return persist_preferences_begin(g_alarms_prefs, PERSIST_PREFS_DOMAIN_APP_ALARMS, read_only);
}

static bool persist_calibration_open(bool read_only)
{
    return persist_preferences_begin(g_calibration_prefs, PERSIST_PREFS_DOMAIN_SENSOR_CALIBRATION, read_only);
}

bool durable_app_state_backend_ready_internal(void)
{
    bool settings_ready;
    bool alarms_ready;
    bool calibration_ready;
    if (!durable_app_state_enabled()) {
        return false;
    }
    settings_ready = persist_settings_open(true);
    if (settings_ready) {
        g_settings_prefs.end();
    }
    alarms_ready = persist_alarms_open(true);
    if (alarms_ready) {
        g_alarms_prefs.end();
    }
    calibration_ready = persist_calibration_open(true);
    if (calibration_ready) {
        g_calibration_prefs.end();
    }
    return settings_ready && alarms_ready && calibration_ready;
}

void durable_migrate_or_mirror(void)
{
    SettingsState settings = {};
    AlarmState alarms[APP_MAX_ALARMS] = {};
    SensorCalibrationData calibration = {};

    if (!durable_app_state_backend_ready_internal()) {
        return;
    }

    const bool has_settings = durable_load_settings(&settings);
    const bool has_alarms = durable_load_alarms(alarms, APP_MAX_ALARMS);
    const bool has_cal = durable_load_calibration(&calibration);
    const bool durable_complete = has_settings && has_alarms && has_cal;

    if (durable_complete) {
        persist_save_settings(&settings);
        persist_save_alarms(alarms, APP_MAX_ALARMS);
        persist_save_sensor_calibration(&calibration);
        write_crc();
        return;
    }

    persist_load_settings(&settings);
    persist_load_alarms(alarms, APP_MAX_ALARMS);
    persist_load_sensor_calibration(&calibration);

    (void)durable_store_settings(&settings);
    (void)durable_store_alarms(alarms, APP_MAX_ALARMS);
    (void)durable_store_calibration(&calibration);
    rtc_mirror_settings(&settings);
    rtc_mirror_alarms(alarms, APP_MAX_ALARMS);
    rtc_mirror_calibration(&calibration);
    write_crc();
}
