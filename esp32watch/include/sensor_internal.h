#ifndef SENSOR_INTERNAL_H
#define SENSOR_INTERNAL_H

#include "sensor.h"
#include "app_config.h"
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SENSOR_INIT_STATE_COLD = 0,
    SENSOR_INIT_STATE_INITING,
    SENSOR_INIT_STATE_BACKOFF,
    SENSOR_INIT_STATE_ONLINE
} SensorInitStateKind;

typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
} SensorSample;

typedef struct {
    SensorInitStateKind state;
    bool initialized;
    bool online;
    uint8_t error_code;
    uint8_t fault_count;
    uint8_t reinit_count;
    uint32_t retry_backoff_ms;
    uint32_t last_attempt_ms;
    uint32_t offline_since_ms;
} SensorInitRuntime;

typedef struct {
    bool calibrated;
    bool pending;
    bool dirty;
    bool persisting;
    bool persist_failed;
    uint8_t progress;
    uint8_t count;
    int32_t sum_az;
    int32_t sum_gx;
    int32_t sum_gy;
    int32_t sum_gz;
    int16_t az_bias;
    int16_t gx_bias;
    int16_t gy_bias;
    int16_t gz_bias;
} SensorCalibrationRuntime;

typedef struct {
    bool static_now;
    int16_t raw_norm_mg;
    int16_t filtered_norm_mg;
    int16_t baseline_mg;
    int16_t deviation_mg;
    int16_t gyro_energy;
    int16_t motion_score;
    uint8_t quality;
    bool pose_initialized;
    float pose_q0;
    float pose_q1;
    float pose_q2;
    float pose_q3;
    float integral_fb_x;
    float integral_fb_y;
    float integral_fb_z;
    float pitch_estimate_deg;
    float roll_estimate_deg;
    float yaw_estimate_deg;
    int8_t pitch_deg;
    int8_t roll_deg;
    int16_t yaw_deg;
} SensorSignalRuntime;

typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    bool static_now;
    int16_t filtered_norm_mg;
    int16_t baseline_mg;
    int16_t deviation_mg;
    int16_t gyro_energy;
    int16_t motion_score;
    uint8_t quality;
    int8_t pitch_deg;
    int8_t roll_deg;
    int16_t yaw_deg;
} SensorProcessedSample;

typedef struct {
    SensorInitRuntime init;
    SensorCalibrationRuntime calibration;
    SensorSignalRuntime signal;
    uint8_t sensitivity_level;
    SensorSnapshot snap;
} SensorStateInternal;

extern SensorStateInternal g_sensor;

void sensor_publish_update(void);
void sensor_reset_runtime_state(void);
void sensor_mark_offline(uint8_t error_code, uint32_t now_ms);
bool sensor_try_init(void);
int16_t sensor_abs16_local(int16_t v);
int32_t sensor_abs32_local(int32_t v);
void sensor_i2c_config_output_od(void);
void sensor_i2c_config_peripheral(void);
void sensor_recover_i2c_bus(void);
bool mpu_write(uint8_t reg, uint8_t value);
bool mpu_read(uint8_t reg, uint8_t *buf, uint16_t len);
void sensor_sync_public_states(void);

void sensor_calibration_load_into_runtime(SensorStateInternal *sensor, const SensorCalibrationData *cal);
void sensor_calibration_request_restart(SensorStateInternal *sensor);
void sensor_calibration_clear_runtime(SensorStateInternal *sensor);
void sensor_calibration_update_window(SensorStateInternal *sensor, const SensorSample *raw, bool static_now);
void sensor_calibration_reset_window(SensorStateInternal *sensor);
bool sensor_calibration_take_dirty(SensorStateInternal *sensor, SensorCalibrationData *out);
void sensor_calibration_note_persisted(SensorStateInternal *sensor);
void sensor_calibration_note_persist_failed(SensorStateInternal *sensor);

void sensor_filter_process_sample(SensorStateInternal *sensor,
                                  const SensorSample *raw,
                                  float dt_s,
                                  SensorProcessedSample *out);

#ifdef __cplusplus
}
#endif

#endif
