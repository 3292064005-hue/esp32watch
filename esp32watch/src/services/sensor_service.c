#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/storage_facade.h"
#include <stddef.h>

static bool g_sensor_service_initialized;

void sensor_service_init(void)
{
    SensorCalibrationData cal;
    sensor_init();
    storage_facade_load_sensor_calibration(&cal);
    sensor_load_calibration(&cal);
    g_sensor_service_initialized = sensor_get_snapshot() != NULL;
}

void sensor_service_tick(uint32_t now_ms)
{
    SensorCalibrationData cal;
    sensor_tick(now_ms);
    if (sensor_take_calibration_dirty(&cal)) {
        storage_facade_save_sensor_calibration(&cal);
        storage_service_request_commit(STORAGE_COMMIT_REASON_CALIBRATION);
    }
}

const SensorSnapshot *sensor_service_get_snapshot(void)
{
    return sensor_get_snapshot();
}

bool sensor_service_force_reinit(void)
{
    return sensor_force_reinit();
}

void sensor_service_request_calibration(void)
{
    sensor_request_calibration();
}

void sensor_service_clear_calibration(void)
{
    sensor_clear_calibration();
}

void sensor_service_set_sensitivity(uint8_t level)
{
    sensor_set_sensitivity(level);
}

void sensor_service_note_storage_commit_result(StorageCommitReason reason, bool ok)
{
    if (reason != STORAGE_COMMIT_REASON_CALIBRATION) {
        return;
    }
    if (ok) {
        sensor_note_calibration_persisted();
    } else {
        sensor_note_calibration_persist_failed();
    }
}

SensorRuntimeState sensor_service_get_runtime_state(void)
{
    return sensor_get_snapshot()->runtime_state;
}

SensorCalibrationState sensor_service_get_calibration_state(void)
{
    return sensor_get_snapshot()->calibration_state;
}

const char *sensor_service_runtime_state_name(void)
{
    return sensor_runtime_state_name(sensor_service_get_runtime_state());
}

const char *sensor_service_calibration_state_name(void)
{
    return sensor_calibration_state_name(sensor_service_get_calibration_state());
}


bool sensor_service_is_initialized(void)
{
    return g_sensor_service_initialized;
}
