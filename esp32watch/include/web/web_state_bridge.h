#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool wifi_connected;
    char ip[24];
    int32_t rssi;
    uint32_t uptime_ms;

    bool app_ready;
    bool sleeping;
    bool animating;
    char current_page[16];
    char time_source[24];

    uint32_t steps;
    uint32_t goal;
    uint8_t goal_percent;

    bool sensor_online;
    bool sensor_calibrated;
    bool sensor_static_now;
    uint8_t sensor_quality;
    uint8_t sensor_error_code;
    uint8_t sensor_fault_count;
    uint8_t sensor_reinit_count;
    uint8_t sensor_calibration_progress;
    char sensor_runtime_state[24];
    char sensor_calibration_state[24];
    int16_t sensor_ax;
    int16_t sensor_ay;
    int16_t sensor_az;
    int16_t sensor_gx;
    int16_t sensor_gy;
    int16_t sensor_gz;
    int16_t sensor_accel_norm_mg;
    int16_t sensor_baseline_mg;
    int16_t sensor_motion_score;
    uint32_t sensor_last_sample_ms;
    uint32_t sensor_steps_total;
    float pitch_deg;
    float roll_deg;

    bool safe_mode_active;
    bool safe_mode_can_clear;
    char safe_mode_reason[32];

    bool has_last_fault;
    char last_fault_name[32];
    char last_fault_severity[24];
    uint8_t last_fault_count;

    char storage_backend[24];
    char storage_commit_state[24];
    bool storage_transaction_active;
    bool storage_sleep_flush_pending;

    uint32_t display_present_count;
    uint32_t display_tx_fail_count;
    bool weather_valid;
    int16_t weather_temperature_tenths_c;
    char weather_text[20];
    char weather_location[24];
    uint32_t weather_updated_at_ms;

    bool has_last_log;
    char last_log_name[32];
    uint16_t last_log_value;
    uint8_t last_log_aux;

    char last_overlay_text[64];
    bool overlay_active;
    uint32_t overlay_expire_at_ms;
} WebStateSnapshot;

bool web_state_snapshot_collect(WebStateSnapshot *out);

#ifdef __cplusplus
}
#endif
