#include "sensor_internal.h"
#include "app_tuning.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int16_t sensor_abs16_local(int16_t v)
{
    return v < 0 ? (int16_t)(-v) : v;
}

int32_t sensor_abs32_local(int32_t v)
{
    return v < 0 ? -v : v;
}

static float sensor_clampf_local(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float sensor_wrap_degrees_local(float angle_deg)
{
    while (angle_deg > 180.0f) {
        angle_deg -= 360.0f;
    }
    while (angle_deg < -180.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

static void sensor_normalize_quaternion_local(float *q0, float *q1, float *q2, float *q3)
{
    float norm;

    if (q0 == NULL || q1 == NULL || q2 == NULL || q3 == NULL) {
        return;
    }

    norm = sqrtf((*q0 * *q0) + (*q1 * *q1) + (*q2 * *q2) + (*q3 * *q3));
    if (norm <= 0.000001f) {
        *q0 = 1.0f;
        *q1 = 0.0f;
        *q2 = 0.0f;
        *q3 = 0.0f;
        return;
    }

    *q0 /= norm;
    *q1 /= norm;
    *q2 /= norm;
    *q3 /= norm;
}

static void sensor_filter_update_pose_output(SensorSignalRuntime *signal)
{
    float vx;
    float vy;
    float vz;
    float yaw_num;
    float yaw_den;

    if (signal == NULL) {
        return;
    }

    vx = 2.0f * ((signal->pose_q1 * signal->pose_q3) - (signal->pose_q0 * signal->pose_q2));
    vy = 2.0f * ((signal->pose_q0 * signal->pose_q1) + (signal->pose_q2 * signal->pose_q3));
    vz = (signal->pose_q0 * signal->pose_q0) - (signal->pose_q1 * signal->pose_q1) -
         (signal->pose_q2 * signal->pose_q2) + (signal->pose_q3 * signal->pose_q3);

    signal->pitch_estimate_deg = atan2f(-vx, sqrtf(vy * vy + vz * vz)) * (180.0f / (float)M_PI);
    signal->roll_estimate_deg = atan2f(vy, vz) * (180.0f / (float)M_PI);
    yaw_num = 2.0f * ((signal->pose_q0 * signal->pose_q3) + (signal->pose_q1 * signal->pose_q2));
    yaw_den = 1.0f - 2.0f * ((signal->pose_q2 * signal->pose_q2) + (signal->pose_q3 * signal->pose_q3));
    signal->yaw_estimate_deg = sensor_wrap_degrees_local(atan2f(yaw_num, yaw_den) * (180.0f / (float)M_PI));
    signal->pitch_estimate_deg = sensor_clampf_local(signal->pitch_estimate_deg, -90.0f, 90.0f);
    signal->roll_estimate_deg = sensor_clampf_local(signal->roll_estimate_deg, -90.0f, 90.0f);
    signal->pitch_deg = (int8_t)APP_CLAMP((int)lroundf(signal->pitch_estimate_deg), -90, 90);
    signal->roll_deg = (int8_t)APP_CLAMP((int)lroundf(signal->roll_estimate_deg), -90, 90);
    signal->yaw_deg = (int16_t)lroundf(signal->yaw_estimate_deg);
}

static void sensor_filter_bootstrap_pose(SensorSignalRuntime *signal, const SensorProcessedSample *out)
{
    float ax;
    float ay;
    float az;
    float accel_norm;
    float pitch_rad;
    float roll_rad;
    float half_pitch;
    float half_roll;
    float cos_pitch;
    float sin_pitch;
    float cos_roll;
    float sin_roll;

    if (signal == NULL || out == NULL) {
        return;
    }

    ax = (float)out->ax;
    ay = (float)out->ay;
    az = (float)out->az;
    accel_norm = sqrtf(ax * ax + ay * ay + az * az);

    if (accel_norm <= 1.0f) {
        signal->pose_initialized = true;
        signal->pose_q0 = 1.0f;
        signal->pose_q1 = 0.0f;
        signal->pose_q2 = 0.0f;
        signal->pose_q3 = 0.0f;
        signal->integral_fb_x = 0.0f;
        signal->integral_fb_y = 0.0f;
        signal->integral_fb_z = 0.0f;
        signal->pitch_estimate_deg = 0.0f;
        signal->roll_estimate_deg = 0.0f;
        signal->yaw_estimate_deg = 0.0f;
        signal->pitch_deg = 0;
        signal->roll_deg = 0;
        signal->yaw_deg = 0;
        return;
    }

    ax /= accel_norm;
    ay /= accel_norm;
    az /= accel_norm;
    pitch_rad = atan2f(-ax, sqrtf(ay * ay + az * az));
    roll_rad = atan2f(ay, az);
    half_pitch = pitch_rad * 0.5f;
    half_roll = roll_rad * 0.5f;
    cos_pitch = cosf(half_pitch);
    sin_pitch = sinf(half_pitch);
    cos_roll = cosf(half_roll);
    sin_roll = sinf(half_roll);

    signal->pose_initialized = true;
    signal->pose_q0 = cos_roll * cos_pitch;
    signal->pose_q1 = sin_roll * cos_pitch;
    signal->pose_q2 = cos_roll * sin_pitch;
    signal->pose_q3 = -sin_roll * sin_pitch;
    signal->integral_fb_x = 0.0f;
    signal->integral_fb_y = 0.0f;
    signal->integral_fb_z = 0.0f;
    sensor_normalize_quaternion_local(&signal->pose_q0, &signal->pose_q1, &signal->pose_q2, &signal->pose_q3);
    sensor_filter_update_pose_output(signal);
}

static float sensor_filter_compute_accel_trust(const SensorProcessedSample *out, bool static_now)
{
    float ax;
    float ay;
    float az;
    float accel_norm;
    float norm_error_ratio;
    float trust;

    if (out == NULL) {
        return 0.0f;
    }

    ax = (float)out->ax;
    ay = (float)out->ay;
    az = (float)out->az;
    accel_norm = sqrtf(ax * ax + ay * ay + az * az);
    if (accel_norm <= 1.0f) {
        return 0.0f;
    }

    norm_error_ratio = fabsf(accel_norm - 16384.0f) / 16384.0f;
    trust = 1.0f - sensor_clampf_local(norm_error_ratio * 3.2f, 0.0f, 1.0f);
    if (!static_now) {
        trust *= 0.65f;
    }
    return sensor_clampf_local(trust, 0.0f, 1.0f);
}

static int16_t sensor_filter_compute_norm_mg(const SensorSample *raw)
{
    int32_t norm_l1 = sensor_abs32_local(raw->ax) + sensor_abs32_local(raw->ay) + sensor_abs32_local(raw->az);
    return (int16_t)((norm_l1 * 1000L) / 16384L);
}

static bool sensor_filter_detect_static(const SensorSignalRuntime *signal, const SensorSample *raw)
{
    const AppTuningManifest *tuning = app_tuning_manifest_get();

    return (sensor_abs16_local(raw->gx) < tuning->sensor_static_gyro_threshold &&
            sensor_abs16_local(raw->gy) < tuning->sensor_static_gyro_threshold &&
            sensor_abs16_local(raw->gz) < tuning->sensor_static_gyro_threshold &&
            sensor_abs16_local((int16_t)(signal->filtered_norm_mg - 1000)) < tuning->sensor_static_delta_mg);
}

static uint8_t sensor_filter_compute_quality(const SensorCalibrationRuntime *calibration,
                                             bool static_now,
                                             int16_t deviation,
                                             int16_t motion_score)
{
    uint8_t quality = 100U;

    if (!calibration->calibrated) {
        quality = 35U;
    } else if (calibration->pending) {
        quality = 55U;
    } else if (!static_now && deviation > 240) {
        quality = 60U;
    } else if (deviation > 320) {
        quality = 45U;
    } else if (motion_score < 25) {
        quality = 85U;
    }

    return quality;
}

static void sensor_filter_update_mahony_pose(SensorSignalRuntime *signal,
                                             const SensorProcessedSample *out,
                                             bool static_now,
                                             float dt_s)
{
    static const float kGyroLsbPerDegPerSec = 131.0f;
    static const float kRadPerDeg = ((float)M_PI / 180.0f);
    static const float kKpDynamic = 0.55f;
    static const float kKpStatic = 1.85f;
    static const float kKiDynamic = 0.010f;
    static const float kKiStatic = 0.075f;
    static const float kIntegralLimitRadPerSec = 0.35f;
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    float accel_norm;
    float accel_trust;
    float halfvx;
    float halfvy;
    float halfvz;
    float halfex;
    float halfey;
    float halfez;
    float two_kp;
    float two_ki;
    float half_dt;
    float qa;
    float qb;
    float qc;
    float qd;

    if (signal == NULL || out == NULL) {
        return;
    }

    if (!signal->pose_initialized || dt_s <= 0.0f || dt_s > 0.25f) {
        sensor_filter_bootstrap_pose(signal, out);
        return;
    }

    ax = (float)out->ax;
    ay = (float)out->ay;
    az = (float)out->az;
    gx = ((float)out->gx / kGyroLsbPerDegPerSec) * kRadPerDeg;
    gy = ((float)out->gy / kGyroLsbPerDegPerSec) * kRadPerDeg;
    gz = ((float)out->gz / kGyroLsbPerDegPerSec) * kRadPerDeg;
    accel_norm = sqrtf(ax * ax + ay * ay + az * az);
    accel_trust = sensor_filter_compute_accel_trust(out, static_now);

    if (accel_norm > 1.0f && accel_trust > 0.001f) {
        ax /= accel_norm;
        ay /= accel_norm;
        az /= accel_norm;
        halfvx = (signal->pose_q1 * signal->pose_q3) - (signal->pose_q0 * signal->pose_q2);
        halfvy = (signal->pose_q0 * signal->pose_q1) + (signal->pose_q2 * signal->pose_q3);
        halfvz = (signal->pose_q0 * signal->pose_q0) - 0.5f + (signal->pose_q3 * signal->pose_q3);
        halfex = (ay * halfvz) - (az * halfvy);
        halfey = (az * halfvx) - (ax * halfvz);
        halfez = (ax * halfvy) - (ay * halfvx);
        two_kp = 2.0f * (static_now ? kKpStatic : kKpDynamic) * accel_trust;
        two_ki = 2.0f * (static_now ? kKiStatic : kKiDynamic) * accel_trust;

        signal->integral_fb_x += two_ki * halfex * dt_s;
        signal->integral_fb_y += two_ki * halfey * dt_s;
        signal->integral_fb_z += two_ki * halfez * dt_s;
        signal->integral_fb_x = sensor_clampf_local(signal->integral_fb_x,
                                                    -kIntegralLimitRadPerSec,
                                                    kIntegralLimitRadPerSec);
        signal->integral_fb_y = sensor_clampf_local(signal->integral_fb_y,
                                                    -kIntegralLimitRadPerSec,
                                                    kIntegralLimitRadPerSec);
        signal->integral_fb_z = sensor_clampf_local(signal->integral_fb_z,
                                                    -kIntegralLimitRadPerSec,
                                                    kIntegralLimitRadPerSec);

        gx += signal->integral_fb_x + (two_kp * halfex);
        gy += signal->integral_fb_y + (two_kp * halfey);
        gz += signal->integral_fb_z + (two_kp * halfez);
    } else {
        signal->integral_fb_x *= 0.98f;
        signal->integral_fb_y *= 0.98f;
        signal->integral_fb_z *= 0.98f;
    }

    half_dt = 0.5f * dt_s;
    gx *= half_dt;
    gy *= half_dt;
    gz *= half_dt;
    qa = signal->pose_q0;
    qb = signal->pose_q1;
    qc = signal->pose_q2;
    qd = signal->pose_q3;

    signal->pose_q0 += (-qb * gx) - (qc * gy) - (qd * gz);
    signal->pose_q1 += (qa * gx) + (qc * gz) - (qd * gy);
    signal->pose_q2 += (qa * gy) - (qb * gz) + (qd * gx);
    signal->pose_q3 += (qa * gz) + (qb * gy) - (qc * gx);
    sensor_normalize_quaternion_local(&signal->pose_q0,
                                      &signal->pose_q1,
                                      &signal->pose_q2,
                                      &signal->pose_q3);
    sensor_filter_update_pose_output(signal);
}

void sensor_filter_process_sample(SensorStateInternal *sensor,
                                  const SensorSample *raw,
                                  float dt_s,
                                  SensorProcessedSample *out)
{
    SensorSignalRuntime *signal;
    const SensorCalibrationRuntime *calibration;

    if (sensor == NULL || raw == NULL || out == NULL) {
        return;
    }

    signal = &sensor->signal;
    calibration = &sensor->calibration;

    signal->raw_norm_mg = sensor_filter_compute_norm_mg(raw);
    signal->filtered_norm_mg = (int16_t)((signal->filtered_norm_mg * 3 + signal->raw_norm_mg) / 4);
    signal->static_now = sensor_filter_detect_static(signal, raw);

    out->ax = raw->ax;
    out->ay = raw->ay;
    out->az = (int16_t)(raw->az - calibration->az_bias);
    out->gx = (int16_t)(raw->gx - calibration->gx_bias);
    out->gy = (int16_t)(raw->gy - calibration->gy_bias);
    out->gz = (int16_t)(raw->gz - calibration->gz_bias);

    if (signal->static_now) {
        signal->baseline_mg = (int16_t)((signal->baseline_mg * 7 + signal->filtered_norm_mg) / 8);
    }

    signal->deviation_mg = sensor_abs16_local((int16_t)(signal->filtered_norm_mg - signal->baseline_mg));
    signal->gyro_energy = (int16_t)((sensor_abs16_local(out->gx) + sensor_abs16_local(out->gy) + sensor_abs16_local(out->gz)) / 12);
    signal->motion_score = (int16_t)((signal->deviation_mg * 3 + signal->gyro_energy * 2) / 5);
    signal->quality = sensor_filter_compute_quality(calibration,
                                                    signal->static_now,
                                                    signal->deviation_mg,
                                                    signal->motion_score);
    sensor_filter_update_mahony_pose(signal, out, signal->static_now, dt_s);

    out->static_now = signal->static_now;
    out->filtered_norm_mg = signal->filtered_norm_mg;
    out->baseline_mg = signal->baseline_mg;
    out->deviation_mg = signal->deviation_mg;
    out->gyro_energy = signal->gyro_energy;
    out->motion_score = signal->motion_score;
    out->quality = signal->quality;
    out->pitch_deg = signal->pitch_deg;
    out->roll_deg = signal->roll_deg;
    out->yaw_deg = signal->yaw_deg;
}
