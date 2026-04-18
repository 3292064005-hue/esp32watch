#ifndef SENSOR_H
#define SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SENSOR_STATE_UNINIT = 0,
    SENSOR_STATE_INITING,
    SENSOR_STATE_READY,
    SENSOR_STATE_CALIBRATING,
    SENSOR_STATE_DEGRADED,
    SENSOR_STATE_BACKOFF,
    SENSOR_STATE_FATAL
} SensorRuntimeState;

typedef enum {
    SENSOR_CAL_STATE_IDLE = 0,
    SENSOR_CAL_STATE_REQUIRED,
    SENSOR_CAL_STATE_COLLECTING,
    SENSOR_CAL_STATE_SOLVED,
    SENSOR_CAL_STATE_PERSISTING,
    SENSOR_CAL_STATE_VALID,
    SENSOR_CAL_STATE_FAILED
} SensorCalibrationState;

typedef struct {
    bool valid;
    int16_t az_bias;
    int16_t gx_bias;
    int16_t gy_bias;
    int16_t gz_bias;
} SensorCalibrationData;

typedef struct SensorSnapshot {
    bool online;
    SensorRuntimeState runtime_state;
    SensorCalibrationState calibration_state;
    bool calibrated;
    bool calibration_pending;
    bool calibration_dirty;
    bool wrist_raise;
    bool static_now;
    uint32_t last_sample_ms;
    uint32_t offline_since_ms;
    uint32_t update_seq;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t accel_norm_mg;
    int16_t baseline_mg;
    int16_t motion_score;
    int8_t pitch_deg;
    int8_t roll_deg;
    int16_t yaw_deg;
    uint8_t activity_level;
    uint8_t quality;
    uint8_t calibration_progress;
    uint8_t fault_count;
    uint8_t reinit_count;
    uint8_t error_code;
    MotionState motion_state;
    uint32_t steps_total;
    uint32_t retry_backoff_ms;
} SensorSnapshot;

void sensor_init(void);
void sensor_tick(uint32_t now_ms);
const SensorSnapshot *sensor_get_snapshot(void);
bool sensor_force_reinit(void);
void sensor_request_calibration(void);
void sensor_clear_calibration(void);
void sensor_set_sensitivity(uint8_t level);
const char *sensor_runtime_state_name(SensorRuntimeState state);
const char *sensor_calibration_state_name(SensorCalibrationState state);
void sensor_note_calibration_persisted(void);
void sensor_note_calibration_persist_failed(void);
void sensor_load_calibration(const SensorCalibrationData *cal);
bool sensor_take_calibration_dirty(SensorCalibrationData *out);

#ifdef __cplusplus
}
#endif

#endif
