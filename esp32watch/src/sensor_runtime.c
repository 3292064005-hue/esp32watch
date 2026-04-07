#include "sensor_internal.h"
#include "services/diag_service.h"
#include "drv_mpu6050.h"
#include "app_tuning.h"
#include <string.h>
#include <stdio.h>

#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define SENSOR_FATAL_FAULT_THRESHOLD 8U

SensorStateInternal g_sensor;

static SensorRuntimeState sensor_compute_runtime_state(void)
{
    const AppTuningManifest *tuning = app_tuning_manifest_get();

    if (g_sensor.init.state == SENSOR_INIT_STATE_INITING) {
        return SENSOR_STATE_INITING;
    }
    if (g_sensor.init.state == SENSOR_INIT_STATE_BACKOFF) {
        if (g_sensor.init.fault_count >= SENSOR_FATAL_FAULT_THRESHOLD &&
            g_sensor.init.retry_backoff_ms >= tuning->sensor_reinit_backoff_max_ms) {
            return SENSOR_STATE_FATAL;
        }
        return SENSOR_STATE_BACKOFF;
    }
    if (g_sensor.init.state != SENSOR_INIT_STATE_ONLINE || !g_sensor.init.online) {
        return SENSOR_STATE_UNINIT;
    }
    if (g_sensor.calibration.pending) {
        return SENSOR_STATE_CALIBRATING;
    }
    if (!g_sensor.calibration.calibrated) {
        return SENSOR_STATE_DEGRADED;
    }
    return SENSOR_STATE_READY;
}

static SensorCalibrationState sensor_compute_calibration_state(void)
{
    if (g_sensor.calibration.persisting) {
        return SENSOR_CAL_STATE_PERSISTING;
    }
    if (g_sensor.calibration.persist_failed) {
        return SENSOR_CAL_STATE_FAILED;
    }
    if (g_sensor.calibration.dirty) {
        return SENSOR_CAL_STATE_SOLVED;
    }
    if (g_sensor.calibration.pending) {
        if (g_sensor.calibration.count == 0U) {
            return SENSOR_CAL_STATE_REQUIRED;
        }
        return SENSOR_CAL_STATE_COLLECTING;
    }
    if (g_sensor.calibration.calibrated) {
        return SENSOR_CAL_STATE_VALID;
    }
    return SENSOR_CAL_STATE_IDLE;
}

void sensor_sync_public_states(void)
{
    g_sensor.snap.runtime_state = sensor_compute_runtime_state();
    g_sensor.snap.calibration_state = sensor_compute_calibration_state();
}

static void sensor_snapshot_sync_offline(uint8_t error_code, uint32_t now_ms)
{
    if (g_sensor.init.offline_since_ms == 0U) {
        g_sensor.init.offline_since_ms = now_ms;
    }
    g_sensor.snap.online = false;
    g_sensor.snap.error_code = error_code;
    g_sensor.snap.fault_count = g_sensor.init.fault_count;
    g_sensor.snap.reinit_count = g_sensor.init.reinit_count;
    g_sensor.snap.offline_since_ms = g_sensor.init.offline_since_ms;
    g_sensor.snap.retry_backoff_ms = g_sensor.init.retry_backoff_ms;
    g_sensor.snap.calibrated = g_sensor.calibration.calibrated;
    g_sensor.snap.calibration_pending = g_sensor.calibration.pending;
    g_sensor.snap.calibration_dirty = g_sensor.calibration.dirty;
    sensor_sync_public_states();
}

static void sensor_snapshot_sync_online(const SensorProcessedSample *processed, uint32_t now_ms)
{
    g_sensor.snap.online = true;
    g_sensor.snap.offline_since_ms = 0U;
    g_sensor.snap.calibrated = g_sensor.calibration.calibrated;
    g_sensor.snap.calibration_pending = g_sensor.calibration.pending;
    g_sensor.snap.calibration_dirty = g_sensor.calibration.dirty;
    if (!g_sensor.calibration.pending) {
        g_sensor.snap.calibration_progress = g_sensor.calibration.calibrated ? 100U : 0U;
    }
    g_sensor.snap.wrist_raise = false;
    g_sensor.snap.static_now = processed->static_now;
    g_sensor.snap.last_sample_ms = now_ms;
    g_sensor.snap.ax = processed->ax;
    g_sensor.snap.ay = processed->ay;
    g_sensor.snap.az = processed->az;
    g_sensor.snap.gx = processed->gx;
    g_sensor.snap.gy = processed->gy;
    g_sensor.snap.gz = processed->gz;
    g_sensor.snap.accel_norm_mg = processed->filtered_norm_mg;
    g_sensor.snap.baseline_mg = processed->baseline_mg;
    g_sensor.snap.motion_score = processed->motion_score;
    g_sensor.snap.pitch_deg = processed->pitch_deg;
    g_sensor.snap.roll_deg = processed->roll_deg;
    g_sensor.snap.yaw_deg = processed->yaw_deg;
    g_sensor.snap.activity_level = 0U;
    g_sensor.snap.motion_state = MOTION_STATE_IDLE;
    g_sensor.snap.quality = processed->quality;
    g_sensor.snap.fault_count = g_sensor.init.fault_count;
    g_sensor.snap.reinit_count = g_sensor.init.reinit_count;
    g_sensor.snap.error_code = g_sensor.init.error_code;
    g_sensor.snap.retry_backoff_ms = g_sensor.init.retry_backoff_ms;
    sensor_sync_public_states();
}

static bool sensor_decode_raw_sample(const uint8_t raw[14], SensorSample *sample)
{
    if (raw == NULL || sample == NULL) {
        return false;
    }

    sample->ax = (int16_t)((raw[0] << 8) | raw[1]);
    sample->ay = (int16_t)((raw[2] << 8) | raw[3]);
    sample->az = (int16_t)((raw[4] << 8) | raw[5]);
    sample->gx = (int16_t)((raw[8] << 8) | raw[9]);
    sample->gy = (int16_t)((raw[10] << 8) | raw[11]);
    sample->gz = (int16_t)((raw[12] << 8) | raw[13]);
    return true;
}

void sensor_publish_update(void)
{
    g_sensor.snap.update_seq++;
}

void sensor_reset_runtime_state(void)
{
    g_sensor.signal.raw_norm_mg = 1000;
    g_sensor.signal.filtered_norm_mg = 1000;
    g_sensor.signal.baseline_mg = 1000;
    g_sensor.signal.deviation_mg = 0;
    g_sensor.signal.gyro_energy = 0;
    g_sensor.signal.motion_score = 0;
    g_sensor.signal.static_now = false;
    g_sensor.signal.quality = 0U;
    g_sensor.signal.pose_initialized = false;
    g_sensor.signal.pose_q0 = 1.0f;
    g_sensor.signal.pose_q1 = 0.0f;
    g_sensor.signal.pose_q2 = 0.0f;
    g_sensor.signal.pose_q3 = 0.0f;
    g_sensor.signal.integral_fb_x = 0.0f;
    g_sensor.signal.integral_fb_y = 0.0f;
    g_sensor.signal.integral_fb_z = 0.0f;
    g_sensor.signal.pitch_estimate_deg = 0.0f;
    g_sensor.signal.roll_estimate_deg = 0.0f;
    g_sensor.signal.yaw_estimate_deg = 0.0f;
    g_sensor.signal.pitch_deg = 0;
    g_sensor.signal.roll_deg = 0;
    g_sensor.signal.yaw_deg = 0;
    g_sensor.snap.activity_level = 0U;
    g_sensor.snap.motion_state = MOTION_STATE_IDLE;
    g_sensor.snap.wrist_raise = false;
    g_sensor.snap.offline_since_ms = 0U;
    g_sensor.snap.retry_backoff_ms = g_sensor.init.retry_backoff_ms;
    sensor_sync_public_states();
}

void sensor_mark_offline(uint8_t error_code, uint32_t now_ms)
{
    g_sensor.init.online = false;
    g_sensor.init.initialized = false;
    g_sensor.init.state = SENSOR_INIT_STATE_BACKOFF;
    g_sensor.init.error_code = error_code;
    if (g_sensor.init.fault_count < 0xFFU) {
        g_sensor.init.fault_count++;
    }
    sensor_snapshot_sync_offline(error_code, now_ms);
    sensor_publish_update();
    diag_service_note_sensor_fault(error_code, g_sensor.init.fault_count);
}

bool sensor_try_init(void)
{
    DrvMpu6050Status st;

    g_sensor.init.state = SENSOR_INIT_STATE_INITING;
    sensor_sync_public_states();
    sensor_recover_i2c_bus();
    printf("[MPU6050] init start\n");

    st = drv_mpu6050_selftest();
    if (st != DRV_MPU6050_STATUS_OK) {
        g_sensor.init.error_code = drv_mpu6050_status_to_error_code(st, 1U);
        printf("[MPU6050] selftest failed, err=%u\n", (unsigned)g_sensor.init.error_code);
        return false;
    }
    st = drv_mpu6050_configure_default();
    if (st != DRV_MPU6050_STATUS_OK) {
        g_sensor.init.error_code = drv_mpu6050_status_to_error_code(st, 3U);
        printf("[MPU6050] config failed, err=%u\n", (unsigned)g_sensor.init.error_code);
        return false;
    }

    g_sensor.init.initialized = true;
    g_sensor.init.online = true;
    g_sensor.init.state = SENSOR_INIT_STATE_ONLINE;
    g_sensor.init.error_code = 0U;
    g_sensor.init.retry_backoff_ms = app_tuning_manifest_get()->sensor_retry_ms;
    g_sensor.init.offline_since_ms = 0U;
    if (g_sensor.init.reinit_count < 0xFFU) {
        g_sensor.init.reinit_count++;
    }
    sensor_reset_runtime_state();
    g_sensor.snap.offline_since_ms = 0U;
    g_sensor.snap.retry_backoff_ms = g_sensor.init.retry_backoff_ms;
    sensor_sync_public_states();
    diag_service_note_sensor_online(g_sensor.init.reinit_count);
    printf("[MPU6050] online, reinit=%u\n", (unsigned)g_sensor.init.reinit_count);
    return true;
}

void sensor_init(void)
{
    memset(&g_sensor, 0, sizeof(g_sensor));
    g_sensor.sensitivity_level = DEFAULT_SENSOR_SENSITIVITY;
    g_sensor.init.retry_backoff_ms = app_tuning_manifest_get()->sensor_retry_ms;
    g_sensor.init.state = SENSOR_INIT_STATE_COLD;
    g_sensor.calibration.pending = true;
    sensor_reset_runtime_state();
    sensor_sync_public_states();
}

bool sensor_force_reinit(void)
{
    g_sensor.init.initialized = false;
    g_sensor.init.online = false;
    g_sensor.init.state = SENSOR_INIT_STATE_COLD;
    g_sensor.init.last_attempt_ms = 0U;
    g_sensor.init.retry_backoff_ms = app_tuning_manifest_get()->sensor_retry_ms;
    sensor_sync_public_states();
    sensor_publish_update();
    return sensor_try_init();
}

void sensor_tick(uint32_t now_ms)
{
#if !APP_FEATURE_SENSOR
    (void)now_ms;
    return;
#else
    uint8_t raw_buf[14];
    SensorSample raw_sample;
    SensorProcessedSample processed;
    float dt_s;

    if (!g_sensor.init.initialized) {
        if ((now_ms - g_sensor.init.last_attempt_ms) < g_sensor.init.retry_backoff_ms) {
            sensor_sync_public_states();
            return;
        }
        g_sensor.init.last_attempt_ms = now_ms;
        if (!sensor_try_init()) {
            sensor_mark_offline(g_sensor.init.error_code, now_ms);
            if (g_sensor.init.retry_backoff_ms < app_tuning_manifest_get()->sensor_reinit_backoff_max_ms) {
                g_sensor.init.retry_backoff_ms *= 2U;
                if (g_sensor.init.retry_backoff_ms > app_tuning_manifest_get()->sensor_reinit_backoff_max_ms) {
                    g_sensor.init.retry_backoff_ms = app_tuning_manifest_get()->sensor_reinit_backoff_max_ms;
                }
            }
            g_sensor.snap.retry_backoff_ms = g_sensor.init.retry_backoff_ms;
            sensor_sync_public_states();
            return;
        }
    }

    if ((now_ms - g_sensor.snap.last_sample_ms) < app_tuning_manifest_get()->sensor_sample_ms) {
        sensor_sync_public_states();
        return;
    }

    if (drv_mpu6050_read_block(MPU6050_REG_ACCEL_XOUT_H, raw_buf, sizeof(raw_buf)) != DRV_MPU6050_STATUS_OK) {
        sensor_mark_offline(8U, now_ms);
        return;
    }

    if (!sensor_decode_raw_sample(raw_buf, &raw_sample)) {
        sensor_mark_offline(9U, now_ms);
        return;
    }

    if (g_sensor.snap.last_sample_ms == 0U || now_ms <= g_sensor.snap.last_sample_ms) {
        dt_s = 0.0f;
    } else {
        dt_s = (float)(now_ms - g_sensor.snap.last_sample_ms) / 1000.0f;
    }

    sensor_filter_process_sample(&g_sensor, &raw_sample, dt_s, &processed);
    if (g_sensor.calibration.pending) {
        sensor_calibration_update_window(&g_sensor, &raw_sample, processed.static_now);
        sensor_filter_process_sample(&g_sensor, &raw_sample, dt_s, &processed);
    }

    g_sensor.init.online = true;
    g_sensor.init.error_code = 0U;
    sensor_snapshot_sync_online(&processed, now_ms);
    sensor_publish_update();
#endif
}

const SensorSnapshot *sensor_get_snapshot(void)
{
    return &g_sensor.snap;
}

const char *sensor_runtime_state_name(SensorRuntimeState state)
{
    switch (state) {
        case SENSOR_STATE_UNINIT: return "UNINIT";
        case SENSOR_STATE_INITING: return "INIT";
        case SENSOR_STATE_READY: return "READY";
        case SENSOR_STATE_CALIBRATING: return "CAL";
        case SENSOR_STATE_DEGRADED: return "DEG";
        case SENSOR_STATE_BACKOFF: return "BACKOFF";
        case SENSOR_STATE_FATAL: return "FATAL";
        default: return "UNK";
    }
}

const char *sensor_calibration_state_name(SensorCalibrationState state)
{
    switch (state) {
        case SENSOR_CAL_STATE_IDLE: return "IDLE";
        case SENSOR_CAL_STATE_REQUIRED: return "REQ";
        case SENSOR_CAL_STATE_COLLECTING: return "COLLECT";
        case SENSOR_CAL_STATE_SOLVED: return "SOLVED";
        case SENSOR_CAL_STATE_PERSISTING: return "SAVE";
        case SENSOR_CAL_STATE_VALID: return "VALID";
        case SENSOR_CAL_STATE_FAILED: return "FAIL";
        default: return "UNK";
    }
}
