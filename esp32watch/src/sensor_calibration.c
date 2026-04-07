#include "sensor_internal.h"
#include "app_tuning.h"
#include <stddef.h>

static SensorCalibrationState sensor_calibration_resolved_state(const SensorStateInternal *sensor)
{
    if (sensor == NULL) {
        return SENSOR_CAL_STATE_IDLE;
    }
    if (sensor->calibration.persisting) {
        return SENSOR_CAL_STATE_PERSISTING;
    }
    if (sensor->calibration.persist_failed) {
        return SENSOR_CAL_STATE_FAILED;
    }
    if (sensor->calibration.dirty) {
        return SENSOR_CAL_STATE_SOLVED;
    }
    if (sensor->calibration.pending) {
        return (sensor->calibration.count == 0U) ? SENSOR_CAL_STATE_REQUIRED : SENSOR_CAL_STATE_COLLECTING;
    }
    if (sensor->calibration.calibrated) {
        return SENSOR_CAL_STATE_VALID;
    }
    return SENSOR_CAL_STATE_IDLE;
}

static void sensor_calibration_publish_state(SensorStateInternal *sensor)
{
    if (sensor == NULL) {
        return;
    }
    sensor->snap.calibrated = sensor->calibration.calibrated;
    sensor->snap.calibration_pending = sensor->calibration.pending;
    sensor->snap.calibration_dirty = sensor->calibration.dirty;
    sensor->snap.calibration_progress = sensor->calibration.progress;
    sensor->snap.calibration_state = sensor_calibration_resolved_state(sensor);
    sensor_sync_public_states();
}

void sensor_calibration_reset_window(SensorStateInternal *sensor)
{
    if (sensor == NULL) {
        return;
    }

    sensor->calibration.count = 0U;
    sensor->calibration.sum_az = 0;
    sensor->calibration.sum_gx = 0;
    sensor->calibration.sum_gy = 0;
    sensor->calibration.sum_gz = 0;
    sensor->calibration.progress = 0U;
    sensor_calibration_publish_state(sensor);
}

void sensor_calibration_load_into_runtime(SensorStateInternal *sensor, const SensorCalibrationData *cal)
{
    if (sensor == NULL) {
        return;
    }

    sensor->calibration.persisting = false;
    sensor->calibration.persist_failed = false;
    if (cal == NULL || !cal->valid) {
        sensor->calibration.calibrated = false;
        sensor->calibration.pending = true;
        sensor->calibration.dirty = false;
        sensor_calibration_reset_window(sensor);
        return;
    }

    sensor->calibration.az_bias = cal->az_bias;
    sensor->calibration.gx_bias = cal->gx_bias;
    sensor->calibration.gy_bias = cal->gy_bias;
    sensor->calibration.gz_bias = cal->gz_bias;
    sensor->calibration.calibrated = true;
    sensor->calibration.pending = false;
    sensor->calibration.dirty = false;
    sensor->calibration.progress = 100U;
    sensor_calibration_publish_state(sensor);
}

void sensor_calibration_request_restart(SensorStateInternal *sensor)
{
    if (sensor == NULL) {
        return;
    }

    sensor->calibration.pending = true;
    sensor->calibration.calibrated = false;
    sensor->calibration.dirty = false;
    sensor->calibration.persisting = false;
    sensor->calibration.persist_failed = false;
    sensor_calibration_reset_window(sensor);
    sensor->snap.retry_backoff_ms = sensor->init.retry_backoff_ms;
    sensor_publish_update();
}

void sensor_calibration_clear_runtime(SensorStateInternal *sensor)
{
    if (sensor == NULL) {
        return;
    }

    sensor->calibration.az_bias = 0;
    sensor->calibration.gx_bias = 0;
    sensor->calibration.gy_bias = 0;
    sensor->calibration.gz_bias = 0;
    sensor->calibration.calibrated = false;
    sensor->calibration.pending = true;
    sensor->calibration.dirty = false;
    sensor->calibration.persisting = false;
    sensor_calibration_reset_window(sensor);
    sensor->snap.retry_backoff_ms = sensor->init.retry_backoff_ms;
    sensor_publish_update();
}

void sensor_calibration_update_window(SensorStateInternal *sensor, const SensorSample *raw, bool static_now)
{
    SensorCalibrationRuntime *calibration;

    if (sensor == NULL || raw == NULL || !sensor->calibration.pending) {
        return;
    }

    calibration = &sensor->calibration;

    if (!static_now) {
        sensor_calibration_reset_window(sensor);
        return;
    }

    calibration->sum_az += raw->az - 16384;
    calibration->sum_gx += raw->gx;
    calibration->sum_gy += raw->gy;
    calibration->sum_gz += raw->gz;
    if (calibration->count < app_tuning_manifest_get()->sensor_calibration_window) {
        calibration->count++;
    }
    calibration->progress = (uint8_t)((calibration->count * 100U) / app_tuning_manifest_get()->sensor_calibration_window);

    if (calibration->count >= app_tuning_manifest_get()->sensor_calibration_window) {
        calibration->az_bias = (int16_t)(calibration->sum_az / (int32_t)calibration->count);
        calibration->gx_bias = (int16_t)(calibration->sum_gx / (int32_t)calibration->count);
        calibration->gy_bias = (int16_t)(calibration->sum_gy / (int32_t)calibration->count);
        calibration->gz_bias = (int16_t)(calibration->sum_gz / (int32_t)calibration->count);
        calibration->calibrated = true;
        calibration->pending = false;
        calibration->dirty = true;
        calibration->persisting = false;
        calibration->persist_failed = false;
        calibration->progress = 100U;
    }

    sensor_calibration_publish_state(sensor);
}

bool sensor_calibration_take_dirty(SensorStateInternal *sensor, SensorCalibrationData *out)
{
    if (sensor == NULL || out == NULL || !sensor->calibration.dirty) {
        return false;
    }

    out->valid = sensor->calibration.calibrated;
    out->az_bias = sensor->calibration.az_bias;
    out->gx_bias = sensor->calibration.gx_bias;
    out->gy_bias = sensor->calibration.gy_bias;
    out->gz_bias = sensor->calibration.gz_bias;
    sensor->calibration.persisting = true;
    sensor->calibration.persist_failed = false;
    sensor->calibration.dirty = false;
    sensor_calibration_publish_state(sensor);
    return true;
}

void sensor_calibration_note_persisted(SensorStateInternal *sensor)
{
    if (sensor == NULL) {
        return;
    }
    sensor->calibration.persisting = false;
    sensor->calibration.persist_failed = false;
    sensor_calibration_publish_state(sensor);
    sensor_publish_update();
}

void sensor_calibration_note_persist_failed(SensorStateInternal *sensor)
{
    if (sensor == NULL) {
        return;
    }
    if (!sensor->calibration.persisting) {
        return;
    }
    sensor->calibration.persisting = false;
    sensor->calibration.persist_failed = true;
    sensor->calibration.dirty = true;
    sensor_calibration_publish_state(sensor);
    sensor_publish_update();
}

void sensor_note_calibration_persisted(void)
{
    sensor_calibration_note_persisted(&g_sensor);
}

void sensor_note_calibration_persist_failed(void)
{
    sensor_calibration_note_persist_failed(&g_sensor);
}

void sensor_load_calibration(const SensorCalibrationData *cal)
{
    sensor_calibration_load_into_runtime(&g_sensor, cal);
}

bool sensor_take_calibration_dirty(SensorCalibrationData *out)
{
    return sensor_calibration_take_dirty(&g_sensor, out);
}

void sensor_set_sensitivity(uint8_t level)
{
    g_sensor.sensitivity_level = level > 2U ? 2U : level;
}

void sensor_request_calibration(void)
{
    sensor_calibration_request_restart(&g_sensor);
}

void sensor_clear_calibration(void)
{
    sensor_calibration_clear_runtime(&g_sensor);
}
