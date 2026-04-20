#ifndef PERSIST_INTERNAL_HPP
#define PERSIST_INTERNAL_HPP

#include <Preferences.h>

extern Preferences g_game_stats_prefs;
extern Preferences g_settings_prefs;
extern Preferences g_alarms_prefs;
extern Preferences g_calibration_prefs;

extern "C" {
#include "persist.h"
}

void read_raw(uint16_t *settings0, uint16_t *settings1, uint16_t *settings2,
              uint16_t *alarm0, uint16_t *alarm1, uint16_t *alarm2, uint16_t *alarm_meta,
              uint16_t *cal0, uint16_t *cal1, uint16_t *cal2, uint16_t *cal3);
void write_crc(void);
bool durable_app_state_enabled(void);
bool persist_game_stats_open(bool read_only);
void rtc_mirror_settings(const SettingsState *settings);
void rtc_mirror_alarms(const AlarmState *alarms, uint8_t count);
void rtc_mirror_calibration(const SensorCalibrationData *cal);
bool durable_store_settings(const SettingsState *settings);
bool durable_load_settings(SettingsState *settings);
bool durable_store_alarms(const AlarmState *alarms, uint8_t count);
bool durable_load_alarms(AlarmState *alarms, uint8_t count);
bool durable_store_calibration(const SensorCalibrationData *cal);
bool durable_load_calibration(SensorCalibrationData *cal);
bool durable_app_state_backend_ready_internal(void);
void durable_migrate_or_mirror(void);

#endif
