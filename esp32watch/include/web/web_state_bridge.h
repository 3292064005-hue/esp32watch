#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "watch_app.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEB_STATE_PERF_RECENT_HISTORY_MAX 6U

typedef struct {
    bool connected;
    bool provisioning_ap_active;
    bool config_wifi_ready;
    bool config_weather_ready;
    bool auth_required;
    char mode[12];
    char status[24];
    char provisioning_ap_ssid[33];
    char ip[24];
    int32_t rssi;
} WebStateWifiSnapshot;

typedef struct {
    uint32_t uptime_ms;
    bool app_ready;
    bool app_degraded;
    char app_init_stage[24];
    bool sleeping;
    bool animating;
    char current_page[16];
    char time_source[24];
    char time_confidence[16];
    char time_authority[16];
    char time_init_status[16];
    char time_init_reason[32];
    bool time_valid;
    bool time_authoritative;
    uint32_t time_source_age_ms;
    bool startup_ok;
    bool startup_degraded;
    bool fatal_stop_requested;
    char startup_failure_stage[24];
    char startup_recovery_stage[24];
    char degraded_code[32];
    char degraded_owner[24];
    char degraded_recovery_hint[96];
    char header_tags[40];
    char system_face[8];
    char brightness_label[12];
} WebStateSystemSnapshot;

typedef struct {
    uint32_t steps;
    uint32_t goal;
    uint8_t goal_percent;
    char activity_label[16];
} WebStateActivitySnapshot;

typedef struct {
    bool valid;
    int16_t temperature_tenths_c;
    char text[20];
    char location[24];
    uint32_t updated_at_ms;
    bool tls_verified;
    bool tls_ca_loaded;
    int32_t last_http_status;
    char tls_mode[16];
    char sync_status[20];
    char network_line[32];
    char network_subline[32];
    char network_label[20];
} WebStateWeatherSnapshot;

typedef struct {
    char sensor_label[16];
    char storage_label[16];
    char diag_label[12];
    char alarm_label[16];
    char music_label[16];
} WebStateLabelSnapshot;

typedef struct {
    WebStateWifiSnapshot wifi;
    WebStateSystemSnapshot system;
    WebStateActivitySnapshot activity;
    WebStateWeatherSnapshot weather;
    WebStateLabelSnapshot labels;
} WebStateCoreSnapshot;

typedef struct {
    bool online;
    bool calibrated;
    bool static_now;
    uint8_t quality;
    uint8_t error_code;
    uint8_t fault_count;
    uint8_t reinit_count;
    uint8_t calibration_progress;
    char runtime_state[24];
    char calibration_state[24];
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t accel_norm_mg;
    int16_t baseline_mg;
    int16_t motion_score;
    uint32_t last_sample_ms;
    uint32_t steps_total;
    float pitch_deg;
    float roll_deg;
} WebStateSensorSnapshot;

typedef struct {
    bool safe_mode_active;
    bool safe_mode_can_clear;
    char safe_mode_reason[32];
    bool has_last_fault;
    char last_fault_name[32];
    char last_fault_severity[24];
    uint8_t last_fault_count;
    bool has_last_log;
    char last_log_name[32];
    uint16_t last_log_value;
    uint8_t last_log_aux;
    uint8_t consecutive_incomplete_boots;
    uint32_t boot_count;
    uint32_t previous_boot_count;
} WebStateDiagSnapshot;

typedef struct {
    char backend[24];
    char backend_phase[24];
    char commit_state[24];
    uint8_t schema_version;
    bool flash_supported;
    bool flash_ready;
    bool migration_attempted;
    bool migration_ok;
    bool transaction_active;
    bool sleep_flush_pending;
    char app_state_backend[24];
    char device_config_backend[24];
    char app_state_durability[20];
    char device_config_durability[20];
    bool app_state_power_loss_guaranteed;
    bool device_config_power_loss_guaranteed;
    bool app_state_mixed_durability;
    uint8_t app_state_reset_domain_object_count;
    uint8_t app_state_durable_object_count;
} WebStateStorageSnapshot;

typedef struct {
    uint32_t present_count;
    uint32_t tx_fail_count;
} WebStateDisplaySnapshot;

typedef struct {
    uint8_t next_alarm_index;
    bool enabled;
    bool ringing;
    char next_time[12];
} WebStateAlarmSnapshot;

typedef struct {
    bool available;
    bool playing;
    char state[16];
    char song[24];
} WebStateMusicSnapshot;

typedef struct {
    char text[64];
    bool active;
    uint32_t expire_at_ms;
} WebStateOverlaySnapshot;

typedef struct {
    char name[16];
    uint32_t budget_ms;
    uint32_t last_duration_ms;
    uint32_t max_duration_ms;
    uint32_t sample_count;
    uint16_t over_budget_count;
    uint16_t consecutive_over_budget;
    uint16_t max_consecutive_over_budget;
    uint16_t deferred_count;
} WebStatePerfStageSnapshot;

typedef struct {
    char stage[16];
    char event[8];
    uint32_t timestamp_ms;
    uint32_t loop_counter;
    uint16_t duration_ms;
    uint16_t budget_ms;
} WebStatePerfHistorySnapshot;

typedef struct {
    uint32_t loop_count;
    uint32_t max_loop_ms;
    uint16_t action_queue_depth;
    uint16_t action_queue_drop_count;
    uint8_t stage_count;
    uint8_t history_count;
    char last_checkpoint[16];
    char last_checkpoint_result[8];
    WebStatePerfStageSnapshot stages[WATCH_APP_STAGE_COUNT];
    WebStatePerfHistorySnapshot history[WEB_STATE_PERF_RECENT_HISTORY_MAX];
} WebStatePerfSnapshot;

typedef struct {
    WebStateSensorSnapshot sensor;
    WebStateDiagSnapshot diag;
    WebStateStorageSnapshot storage;
    WebStateDisplaySnapshot display;
    WebStateAlarmSnapshot alarm;
    WebStateMusicSnapshot music;
    WebStateOverlaySnapshot overlay;
    WebStatePerfSnapshot perf;
} WebStateDetailSnapshot;

void web_state_bridge_mark_startup(uint32_t mark_ms);
bool web_state_core_collect(WebStateCoreSnapshot *out);
bool web_state_detail_collect(WebStateDetailSnapshot *out);
bool web_state_perf_collect(WebStatePerfSnapshot *out);

#ifdef __cplusplus
}
#endif
