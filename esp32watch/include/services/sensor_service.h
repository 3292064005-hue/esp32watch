#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "sensor.h"

void sensor_service_init(void);
void sensor_service_tick(uint32_t now_ms);
const SensorSnapshot *sensor_service_get_snapshot(void);
bool sensor_service_force_reinit(void);
void sensor_service_request_calibration(void);
void sensor_service_clear_calibration(void);
void sensor_service_set_sensitivity(uint8_t level);
void sensor_service_note_storage_commit_result(StorageCommitReason reason, bool ok);
SensorRuntimeState sensor_service_get_runtime_state(void);
SensorCalibrationState sensor_service_get_calibration_state(void);
const char *sensor_service_runtime_state_name(void);
const char *sensor_service_calibration_state_name(void);

bool sensor_service_is_initialized(void);

#endif
