#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>
#include <stdint.h>
#include "app_limits.h"
#include "model.h"
#include "sensor.h"

void persist_init(void);
bool persist_is_initialized(void);
uint8_t persist_get_version(void);
uint16_t persist_get_stored_crc(void);
uint16_t persist_get_calculated_crc(void);
void persist_save_settings(const SettingsState *settings);
void persist_load_settings(SettingsState *settings);
void persist_save_alarms(const AlarmState *alarms, uint8_t count);
void persist_load_alarms(AlarmState *alarms, uint8_t count);
void persist_save_sensor_calibration(const SensorCalibrationData *cal);
void persist_load_sensor_calibration(SensorCalibrationData *cal);

#endif
