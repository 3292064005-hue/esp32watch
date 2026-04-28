#ifndef APP_TUNING_H
#define APP_TUNING_H

#include <stdint.h>
#include "watch_app.h"

#define UI_FPS                          24U
#define UI_FRAME_MS                     (1000U / UI_FPS)
#define UI_ANIM_DURATION_MS             220U
#define UI_POPUP_REFRESH_MS             180U
#define UI_WATCHFACE_REFRESH_MS         1000U
#define UI_CARD_REFRESH_MS              400U
#define UI_TIMER_REFRESH_MS             200U
#define UI_STOPWATCH_REFRESH_MS         80U
#define UI_SENSOR_REFRESH_MS            150U
#define UI_GAME_REFRESH_MS              50U
#define UI_LIQUID_REFRESH_MS            45U

#define BATTERY_SAMPLE_MS               30000U
#define TIME_REFRESH_MS                 200U
#define LOW_BATTERY_THRESHOLD           15U
#define SENSOR_SAMPLE_MS                25U
#define SENSOR_RETRY_MS                 1000U
#define SENSOR_FAULT_POPUP_MS           3000U
#define SENSOR_WRIST_THRESHOLD_MG       220
#define SENSOR_WRIST_GYRO_THRESHOLD     5400
#define SENSOR_WRIST_COOLDOWN_MS        1200U
#define SENSOR_STEP_MIN_INTERVAL_MS     260U
#define SENSOR_STEP_MAX_INTERVAL_MS     1800U
#define SENSOR_STEP_HIGH_THRESHOLD_MG   130
#define SENSOR_STEP_LOW_THRESHOLD_MG    45
#define SENSOR_ACTIVE_DECAY_MS          1800U
#define SENSOR_STATIC_GYRO_THRESHOLD    900
#define SENSOR_STATIC_DELTA_MG          40
#define SENSOR_CALIBRATION_WINDOW       32U
#define SENSOR_QUALITY_GOOD_THRESHOLD   70U
#define SENSOR_REINIT_BACKOFF_MAX_MS    5000U
#define SENSOR_QUALITY_POOR_THRESHOLD   45U
#define SENSOR_WRIST_MIN_PITCH          15
#define SENSOR_WRIST_MAX_PITCH          80
#define SENSOR_WRIST_MAX_ROLL           75
#define ALARM_SNOOZE_MINUTES            10U

#define STORAGE_COMMIT_IDLE_MS         1200U
#define STORAGE_COMMIT_MAX_MS          4000U
#define STORAGE_COMMIT_CAL_MS          250U

#define WATCH_APP_STAGE_INPUT_BUDGET_MS     2U
#define WATCH_APP_STAGE_SENSOR_BUDGET_MS    4U
#define WATCH_APP_STAGE_MODEL_BUDGET_MS     3U
#define WATCH_APP_STAGE_UI_BUDGET_MS        4U
#define WATCH_APP_STAGE_BATTERY_BUDGET_MS   2U
#define WATCH_APP_STAGE_ALERT_BUDGET_MS     2U
#define WATCH_APP_STAGE_DIAG_BUDGET_MS      3U
#define WATCH_APP_STAGE_STORAGE_BUDGET_MS   4U
#define WATCH_APP_STAGE_NETWORK_BUDGET_MS   4U
#define WATCH_APP_STAGE_WEB_BUDGET_MS       4U
#define WATCH_APP_STAGE_RENDER_BUDGET_MS    8U
#define WATCH_APP_STAGE_IDLE_BUDGET_MS      2U

#define APP_STORAGE_VERSION             6U

typedef struct {
    uint32_t ui_fps;
    uint32_t ui_frame_ms;
    uint32_t ui_anim_duration_ms;
    uint32_t ui_popup_refresh_ms;
    uint32_t ui_watchface_refresh_ms;
    uint32_t ui_card_refresh_ms;
    uint32_t ui_timer_refresh_ms;
    uint32_t ui_stopwatch_refresh_ms;
    uint32_t ui_sensor_refresh_ms;
    uint32_t ui_game_refresh_ms;
    uint32_t ui_liquid_refresh_ms;

    uint32_t battery_sample_ms;
    uint32_t time_refresh_ms;
    uint8_t low_battery_threshold;

    uint32_t sensor_sample_ms;
    uint32_t sensor_retry_ms;
    uint32_t sensor_fault_popup_ms;
    int16_t sensor_wrist_threshold_mg;
    int16_t sensor_wrist_gyro_threshold;
    uint32_t sensor_wrist_cooldown_ms;
    uint32_t sensor_step_min_interval_ms;
    uint32_t sensor_step_max_interval_ms;
    int16_t sensor_step_high_threshold_mg;
    int16_t sensor_step_low_threshold_mg;
    uint32_t sensor_active_decay_ms;
    int16_t sensor_static_gyro_threshold;
    int16_t sensor_static_delta_mg;
    uint8_t sensor_calibration_window;
    uint8_t sensor_quality_good_threshold;
    uint8_t sensor_quality_poor_threshold;
    uint32_t sensor_reinit_backoff_max_ms;
    int8_t sensor_wrist_min_pitch;
    int8_t sensor_wrist_max_pitch;
    int8_t sensor_wrist_max_roll;
    int16_t sensor_profile_step_high_threshold_mg[3];
    int16_t sensor_profile_step_low_threshold_mg[3];
    int16_t sensor_profile_wrist_threshold_mg[3];
    uint16_t sensor_profile_warmup_ms[3];
    uint8_t sensor_profile_min_quality[3];

    uint32_t alarm_snooze_minutes;

    uint32_t storage_commit_idle_ms;
    uint32_t storage_commit_max_ms;
    uint32_t storage_commit_cal_ms;
} AppTuningManifest;

static inline const AppTuningManifest *app_tuning_manifest_get(void)
{
    static const AppTuningManifest manifest = {
        .ui_fps = UI_FPS,
        .ui_frame_ms = UI_FRAME_MS,
        .ui_anim_duration_ms = UI_ANIM_DURATION_MS,
        .ui_popup_refresh_ms = UI_POPUP_REFRESH_MS,
        .ui_watchface_refresh_ms = UI_WATCHFACE_REFRESH_MS,
        .ui_card_refresh_ms = UI_CARD_REFRESH_MS,
        .ui_timer_refresh_ms = UI_TIMER_REFRESH_MS,
        .ui_stopwatch_refresh_ms = UI_STOPWATCH_REFRESH_MS,
        .ui_sensor_refresh_ms = UI_SENSOR_REFRESH_MS,
        .ui_game_refresh_ms = UI_GAME_REFRESH_MS,
        .ui_liquid_refresh_ms = UI_LIQUID_REFRESH_MS,

        .battery_sample_ms = BATTERY_SAMPLE_MS,
        .time_refresh_ms = TIME_REFRESH_MS,
        .low_battery_threshold = LOW_BATTERY_THRESHOLD,

        .sensor_sample_ms = SENSOR_SAMPLE_MS,
        .sensor_retry_ms = SENSOR_RETRY_MS,
        .sensor_fault_popup_ms = SENSOR_FAULT_POPUP_MS,
        .sensor_wrist_threshold_mg = SENSOR_WRIST_THRESHOLD_MG,
        .sensor_wrist_gyro_threshold = SENSOR_WRIST_GYRO_THRESHOLD,
        .sensor_wrist_cooldown_ms = SENSOR_WRIST_COOLDOWN_MS,
        .sensor_step_min_interval_ms = SENSOR_STEP_MIN_INTERVAL_MS,
        .sensor_step_max_interval_ms = SENSOR_STEP_MAX_INTERVAL_MS,
        .sensor_step_high_threshold_mg = SENSOR_STEP_HIGH_THRESHOLD_MG,
        .sensor_step_low_threshold_mg = SENSOR_STEP_LOW_THRESHOLD_MG,
        .sensor_active_decay_ms = SENSOR_ACTIVE_DECAY_MS,
        .sensor_static_gyro_threshold = SENSOR_STATIC_GYRO_THRESHOLD,
        .sensor_static_delta_mg = SENSOR_STATIC_DELTA_MG,
        .sensor_calibration_window = SENSOR_CALIBRATION_WINDOW,
        .sensor_quality_good_threshold = SENSOR_QUALITY_GOOD_THRESHOLD,
        .sensor_quality_poor_threshold = SENSOR_QUALITY_POOR_THRESHOLD,
        .sensor_reinit_backoff_max_ms = SENSOR_REINIT_BACKOFF_MAX_MS,
        .sensor_wrist_min_pitch = SENSOR_WRIST_MIN_PITCH,
        .sensor_wrist_max_pitch = SENSOR_WRIST_MAX_PITCH,
        .sensor_wrist_max_roll = SENSOR_WRIST_MAX_ROLL,
        .sensor_profile_step_high_threshold_mg = {
            SENSOR_STEP_HIGH_THRESHOLD_MG + 20,
            SENSOR_STEP_HIGH_THRESHOLD_MG,
            SENSOR_STEP_HIGH_THRESHOLD_MG - 20
        },
        .sensor_profile_step_low_threshold_mg = {
            SENSOR_STEP_LOW_THRESHOLD_MG + 20,
            SENSOR_STEP_LOW_THRESHOLD_MG,
            SENSOR_STEP_LOW_THRESHOLD_MG - 20
        },
        .sensor_profile_wrist_threshold_mg = {
            SENSOR_WRIST_THRESHOLD_MG + 25,
            SENSOR_WRIST_THRESHOLD_MG,
            SENSOR_WRIST_THRESHOLD_MG - 20
        },
        .sensor_profile_warmup_ms = {1200U, 900U, 700U},
        .sensor_profile_min_quality = {
            SENSOR_QUALITY_POOR_THRESHOLD,
            SENSOR_QUALITY_POOR_THRESHOLD,
            SENSOR_QUALITY_POOR_THRESHOLD
        },

        .alarm_snooze_minutes = ALARM_SNOOZE_MINUTES,

        .storage_commit_idle_ms = STORAGE_COMMIT_IDLE_MS,
        .storage_commit_max_ms = STORAGE_COMMIT_MAX_MS,
        .storage_commit_cal_ms = STORAGE_COMMIT_CAL_MS
    };
    return &manifest;
}

#endif
