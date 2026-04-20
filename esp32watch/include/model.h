#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include "app_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MODEL_ENABLE_LEGACY_PROJECTION
#define MODEL_ENABLE_LEGACY_PROJECTION 0
#endif

typedef struct SensorSnapshot SensorSnapshot;

typedef enum {
    POPUP_NONE = 0,
    POPUP_ALARM,
    POPUP_TIMER_DONE,
    POPUP_LOW_BATTERY,
    POPUP_SENSOR_FAULT,
    POPUP_GOAL_REACHED
} PopupType;

typedef enum {
    MOTION_STATE_IDLE = 0,
    MOTION_STATE_LIGHT,
    MOTION_STATE_WALKING,
    MOTION_STATE_ACTIVE
} MotionState;

typedef enum {
    STORAGE_BACKEND_BKP = 0,
    STORAGE_BACKEND_FLASH = 1
} StorageBackendType;

typedef enum {
    RESET_REASON_UNKNOWN = 0,
    RESET_REASON_POWER_ON,
    RESET_REASON_PIN,
    RESET_REASON_SOFTWARE,
    RESET_REASON_IWDG,
    RESET_REASON_WWDG,
    RESET_REASON_LOW_POWER
} ResetReason;

typedef enum {
    WAKE_REASON_NONE = 0,
    WAKE_REASON_BOOT,
    WAKE_REASON_KEY,
    WAKE_REASON_POPUP,
    WAKE_REASON_WRIST_RAISE,
    WAKE_REASON_MANUAL,
    WAKE_REASON_SERVICE
} WakeReason;

typedef enum {
    SLEEP_REASON_NONE = 0,
    SLEEP_REASON_AUTO_TIMEOUT,
    SLEEP_REASON_MANUAL,
    SLEEP_REASON_SERVICE
} SleepReason;

typedef enum {
    STORAGE_COMMIT_REASON_NONE = 0,
    STORAGE_COMMIT_REASON_IDLE,
    STORAGE_COMMIT_REASON_MAX_AGE,
    STORAGE_COMMIT_REASON_CALIBRATION,
    STORAGE_COMMIT_REASON_MANUAL,
    STORAGE_COMMIT_REASON_SLEEP,
    STORAGE_COMMIT_REASON_RESTORE_DEFAULTS,
    STORAGE_COMMIT_REASON_ALARM
} StorageCommitReason;

typedef enum {
    TIME_STATE_UNSET = 0,
    TIME_STATE_VALID,
    TIME_STATE_RECOVERED
} TimeState;

typedef enum {
    GAME_ID_BREAKOUT = 0,
    GAME_ID_DINO,
    GAME_ID_PONG,
    GAME_ID_SNAKE,
    GAME_ID_TETRIS,
    GAME_ID_SHOOTER,
    GAME_ID_COUNT
} GameId;

typedef enum {
    MODEL_RUNTIME_REQUEST_NONE = 0,
    MODEL_RUNTIME_REQUEST_STAGE_SETTINGS = 1 << 0,
    MODEL_RUNTIME_REQUEST_STAGE_ALARMS = 1 << 1,
    MODEL_RUNTIME_REQUEST_STORAGE_COMMIT = 1 << 2,
    MODEL_RUNTIME_REQUEST_APPLY_BRIGHTNESS = 1 << 3,
    MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS = 1 << 4,
    MODEL_RUNTIME_REQUEST_CLEAR_SENSOR_CALIBRATION = 1 << 5,
    MODEL_RUNTIME_REQUEST_STAGE_GAME_STATS = 1 << 6
} ModelRuntimeRequestFlags;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} DateTime;

typedef struct {
    bool enabled;
    bool ringing;
    uint8_t hour;
    uint8_t minute;
    uint8_t repeat_mask;
    uint8_t label_id;
    uint32_t snooze_until_epoch;
    uint16_t last_trigger_day;
} AlarmState;

typedef struct {
    bool running;
    uint32_t elapsed_ms;
    uint32_t last_tick_ms;
    uint32_t laps[APP_STOPWATCH_LAP_MAX];
    uint8_t lap_count;
} StopwatchState;

typedef struct {
    bool running;
    uint32_t preset_s;
    uint32_t remain_s;
    uint32_t last_tick_ms;
    uint8_t preset_index;
} TimerState;

typedef struct {
    uint32_t steps;
    uint32_t goal;
    uint8_t activity_level;
    MotionState motion_state;
    bool wrist_raise;
    bool goal_reached_today;
    uint16_t active_minutes;
} ActivityState;

typedef struct {
    bool online;
    bool calibrated;
    bool static_now;
    bool calibration_pending;
    uint8_t runtime_state;
    uint8_t calibration_state;
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
    uint8_t quality;
    uint8_t calibration_progress;
    uint8_t fault_count;
    uint8_t reinit_count;
    uint8_t error_code;
    uint32_t steps_total;
    uint32_t last_sample_ms;
    uint32_t offline_since_ms;
    uint32_t last_motion_ms;
    uint32_t retry_backoff_ms;
} SensorState;

typedef struct {
    uint8_t brightness;
    bool vibrate;
    bool auto_wake;
    bool auto_sleep;
    bool dnd;
    bool show_seconds;
    bool animations;
    uint8_t watchface;
    uint8_t screen_timeout_idx;
    uint8_t sensor_sensitivity;
    uint32_t goal;
} SettingsState;

typedef struct {
    uint16_t breakout_hi;
    uint16_t dino_hi;
    uint16_t pong_hi;
    uint16_t snake_hi;
    uint16_t tetris_hi;
    uint16_t shooter_hi;
} GameStatsState;

typedef struct {
    DateTime now;
    bool time_valid;
    TimeState time_state;
    AlarmState alarms[APP_MAX_ALARMS];
    AlarmState alarm; /* selected alarm mirror for legacy/UI convenience */
    uint8_t alarm_selected;
    uint8_t alarm_ringing_index;
    StopwatchState stopwatch;
    TimerState timer;
    ActivityState activity;
    SensorState sensor;
    SettingsState settings;
    GameStatsState game_stats;
    uint16_t battery_mv;
    uint8_t battery_percent;
    bool battery_present;
    bool charging;
    PopupType popup;
    bool popup_latched;
    PopupType popup_queue[APP_POPUP_QUEUE_DEPTH];
    uint8_t popup_queue_count;
    bool sensor_fault_latched;
    bool storage_ok;
    bool storage_crc_ok;
    uint8_t storage_version;
    StorageBackendType storage_backend;
    uint16_t storage_stored_crc;
    uint16_t storage_calc_crc;
    uint32_t storage_last_commit_ms;
    uint32_t storage_commit_count;
    uint8_t storage_pending_mask;
    uint16_t input_queue_overflow_count;
    ResetReason reset_reason;
    WakeReason last_wake_reason;
    SleepReason last_sleep_reason;
    uint32_t last_sleep_ms;
    uint8_t storage_last_commit_ok;
    StorageCommitReason storage_last_commit_reason;
    uint8_t storage_dirty_source_mask;
    uint32_t last_user_activity_ms;
    uint32_t last_wake_ms;
    uint32_t last_render_ms;
    uint32_t screen_timeout_ms;
    uint16_t current_day_id;
} WatchModel;


typedef struct {
    DateTime now;
    bool time_valid;
    TimeState time_state;
    AlarmState alarms[APP_MAX_ALARMS];
    AlarmState selected_alarm;
    uint8_t alarm_selected;
    uint8_t alarm_ringing_index;
    StopwatchState stopwatch;
    TimerState timer;
    ActivityState activity;
    SettingsState settings;
    GameStatsState game_stats;
    uint16_t current_day_id;
} ModelDomainState;

typedef struct {
    SensorState sensor;
    uint16_t battery_mv;
    uint8_t battery_percent;
    bool battery_present;
    bool charging;
    bool storage_ok;
    bool storage_crc_ok;
    uint8_t storage_version;
    StorageBackendType storage_backend;
    uint16_t storage_stored_crc;
    uint16_t storage_calc_crc;
    uint32_t storage_last_commit_ms;
    uint32_t storage_commit_count;
    uint8_t storage_pending_mask;
    uint16_t input_queue_overflow_count;
    ResetReason reset_reason;
    WakeReason last_wake_reason;
    SleepReason last_sleep_reason;
    uint32_t last_sleep_ms;
    uint8_t storage_last_commit_ok;
    StorageCommitReason storage_last_commit_reason;
    uint8_t storage_dirty_source_mask;
    uint32_t last_user_activity_ms;
    uint32_t last_wake_ms;
    uint32_t last_render_ms;
    uint32_t screen_timeout_ms;
} ModelRuntimeState;

typedef struct {
    PopupType popup;
    bool popup_latched;
    PopupType popup_queue[APP_POPUP_QUEUE_DEPTH];
    uint8_t popup_queue_count;
    bool sensor_fault_latched;
    bool has_runtime_requests;
} ModelUiState;

void model_init(void);
void model_tick(uint32_t now_ms);
void model_sync_runtime_observability(void);
/**
 * @brief Normalize split-state derived fields before callers snapshot read-side views.
 *
 * @param None.
 * @return void
 * @throws None.
 * @boundary_behavior Repairs derived selected-alarm and popup/runtime-request mirrors before returning.
 */
void model_flush_read_snapshots(void);

/**
 * @brief Return a synthesized aggregate snapshot as a read-only compatibility view.
 *
 * @return Pointer to an internally owned synthesized compatibility snapshot. The
 *         returned snapshot is read-only; all authoritative writes must go through
 *         the split-state mutation helpers.
 * @throws None.
 */
const WatchModel *model_get(void);

/**
 * @brief Build a read-only snapshot of persistent domain state.
 *
 * @param[out] out Optional destination for the copied domain state.
 * @return Pointer to the internal snapshot copy or NULL when @p out is NULL.
 * @throws None.
 */
const ModelDomainState *model_get_domain_state(ModelDomainState *out);

/**
 * @brief Build a read-only snapshot of runtime observability state.
 *
 * @param[out] out Optional destination for the copied runtime state.
 * @return Pointer to the internal snapshot copy or NULL when @p out is NULL.
 * @throws None.
 */
const ModelRuntimeState *model_get_runtime_state(ModelRuntimeState *out);

/**
 * @brief Build a read-only snapshot of UI-facing transient state.
 *
 * @param[out] out Optional destination for the copied UI state.
 * @return Pointer to the internal snapshot copy or NULL when @p out is NULL.
 * @throws None.
 */
const ModelUiState *model_get_ui_state(ModelUiState *out);

/**
 * @brief Check whether the domain layer has pending runtime side effects.
 *
 * @param None.
 * @return true when at least one runtime request is pending.
 * @throws None.
 */
bool model_has_runtime_requests(void);

/**
 * @brief Consume and clear the current runtime side-effect request bundle.
 *
 * @param[out] commit_reason Receives the requested storage commit reason when not NULL.
 * @return Bitwise OR of ModelRuntimeRequestFlags values.
 * @throws None.
 */
uint32_t model_consume_runtime_requests(StorageCommitReason *commit_reason);


void model_ack_popup(void);
void model_snooze_alarm(void);
/**
 * @brief Restore model-owned app-state defaults without clearing durable game statistics.
 *
 * @return void
 * @throws None.
 */
void model_restore_defaults(void);

/**
 * @brief Restore factory defaults for model-owned app state and durable game statistics.
 *
 * @return void
 * @throws None.
 * @boundary_behavior Schedules settings, alarms, sensor-calibration clear, and game-stat persistence through the normal runtime/storage pipeline so factory reset semantics stay aligned with storage ownership metadata.
 */
void model_factory_reset_defaults(void);

void model_push_popup(PopupType popup, bool priority);
bool model_popup_queue_contains(PopupType popup);

uint8_t model_get_alarm_count(void);
const AlarmState *model_get_alarm(uint8_t index);
uint8_t model_get_selected_alarm_index(void);
void model_select_alarm(uint8_t index);
void model_select_alarm_offset(int8_t delta);
uint8_t model_find_next_enabled_alarm(void);

void model_set_alarm_enabled(bool enabled);
void model_set_alarm_enabled_at(uint8_t index, bool enabled);
void model_set_alarm_time(uint8_t hour, uint8_t minute);
void model_set_alarm_time_at(uint8_t index, uint8_t hour, uint8_t minute);
void model_set_alarm_repeat_mask_at(uint8_t index, uint8_t repeat_mask);

void model_stopwatch_toggle(uint32_t now_ms);
void model_stopwatch_reset(void);
void model_stopwatch_lap(void);

void model_timer_toggle(uint32_t now_ms);
void model_timer_reset(void);
void model_timer_adjust_seconds(int32_t delta_s);
void model_timer_set_preset(uint32_t preset_s);
void model_timer_cycle_preset(int8_t dir);

void model_set_watchface(uint8_t face);
void model_cycle_watchface(int8_t dir);
void model_set_brightness(uint8_t level);
void model_set_vibrate(bool enabled);
void model_set_auto_wake(bool enabled);
void model_set_auto_sleep(bool enabled);
void model_set_dnd(bool enabled);
void model_set_goal(uint32_t goal);
void model_set_show_seconds(bool enabled);
void model_set_animations(bool enabled);
void model_set_sensor_sensitivity(uint8_t level);
void model_set_screen_timeout_idx(uint8_t idx);
void model_cycle_screen_timeout(int8_t dir);
bool model_set_game_high_score(GameId game_id, uint16_t score);
uint16_t model_get_game_high_score(GameId game_id);
const GameStatsState *model_get_game_stats(GameStatsState *out);

void model_update_sensor_raw(const SensorSnapshot *snap, uint32_t now_ms);
void model_update_activity(bool wrist_raise, uint8_t activity_level,
                           MotionState motion_state, uint16_t steps_inc,
                           uint32_t now_ms);

void model_note_user_activity(void);
void model_note_render(uint32_t now_ms);
void model_note_manual_wake(uint32_t now_ms);

void model_set_battery(uint16_t mv, uint8_t percent, bool charging, bool present);

uint32_t model_datetime_to_epoch(const DateTime *dt);
void model_epoch_to_datetime(uint32_t epoch, DateTime *out);
void model_set_datetime(const DateTime *dt);

const char *model_reset_reason_name(ResetReason reason);
const char *model_wake_reason_name(WakeReason reason);
const char *model_sleep_reason_name(SleepReason reason);
const char *model_storage_commit_reason_name(StorageCommitReason reason);
const char *model_time_state_name(TimeState state);

#ifdef __cplusplus
}
#endif

#endif
