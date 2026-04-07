#ifndef WATCH_APP_INTERNAL_H
#define WATCH_APP_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "watch_app.h"
#include "model.h"
#include "sensor.h"
#include "services/wdt_service.h"

#define WATCH_APP_STAGE_HISTORY_CAPACITY 16U

typedef struct {
    bool pending;
    SleepReason reason;
    uint32_t requested_at_ms;
} WatchAppSleepRequestState;

typedef struct {
    uint32_t last_key_scan;
    uint8_t last_sensor_sensitivity;
    uint32_t last_storage_commit_ms;
    uint32_t loop_counter;
    bool qos_render_due;
    bool qos_ui_feedback_pending;
    bool qos_sensor_backoff_active;
    bool qos_alarm_active;
    WatchAppInitReport init_report;
    WatchAppSleepRequestState sleep_request;
    WatchAppStageTelemetry stage_telemetry[WATCH_APP_STAGE_COUNT];
    WatchAppStageHistoryEntry stage_history[WATCH_APP_STAGE_HISTORY_CAPACITY];
    uint8_t stage_history_head;
    uint8_t stage_history_count;
} WatchAppRuntimeState;

/**
 * @brief Reset per-stage telemetry and retained history buffers.
 *
 * @param[out] telemetry Telemetry array sized to WATCH_APP_STAGE_COUNT.
 * @param[out] history Ring buffer storage sized to WATCH_APP_STAGE_HISTORY_CAPACITY.
 * @param[out] head History write cursor.
 * @param[out] count Number of valid history entries.
 * @return void
 * @throws None.
 */
void watch_app_reset_stage_telemetry(WatchAppStageTelemetry *telemetry,
                                     WatchAppStageHistoryEntry *history,
                                     uint8_t *head,
                                     uint8_t *count);

/**
 * @brief Record a stage defer event in telemetry and history.
 *
 * @param[in] stage Stage identifier.
 * @param[in,out] telemetry Telemetry array sized to WATCH_APP_STAGE_COUNT.
 * @param[in,out] history Ring buffer storage.
 * @param[in,out] head History write cursor.
 * @param[in,out] count Number of valid history entries.
 * @param[in] loop_counter Current main-loop iteration counter.
 * @return void
 * @throws None.
 */
void watch_app_note_stage_deferred(WatchAppStageId stage,
                                   WatchAppStageTelemetry *telemetry,
                                   WatchAppStageHistoryEntry *history,
                                   uint8_t *head,
                                   uint8_t *count,
                                   uint32_t loop_counter);

/**
 * @brief Record stage execution timing and over-budget history.
 *
 * @param[in] stage Stage identifier.
 * @param[in] start_ms Stage start tick.
 * @param[in] end_ms Stage end tick.
 * @param[in,out] telemetry Telemetry array sized to WATCH_APP_STAGE_COUNT.
 * @param[in,out] history Ring buffer storage.
 * @param[in,out] head History write cursor.
 * @param[in,out] count Number of valid history entries.
 * @param[in] loop_counter Current main-loop iteration counter.
 * @return void
 * @throws None.
 */
void watch_app_record_stage_timing(WatchAppStageId stage,
                                   uint32_t start_ms,
                                   uint32_t end_ms,
                                   WatchAppStageTelemetry *telemetry,
                                   WatchAppStageHistoryEntry *history,
                                   uint8_t *head,
                                   uint8_t *count,
                                   uint32_t loop_counter);

/**
 * @brief Return the monotonic clock used for runtime-budget stage timing.
 *
 * @param None.
 * @return Monotonic stage-timing tick expressed in milliseconds.
 * @throws None.
 * @boundary_behavior Defaults to platform_time_now_ms() unless a platform/test override is linked.
 */
uint32_t watch_app_stage_clock_now(void);

bool watch_app_should_defer_render(const WatchAppStageTelemetry *telemetry, uint32_t loop_counter);
bool watch_app_should_run_diag_stage(const WatchAppStageTelemetry *telemetry, uint32_t loop_counter);
bool watch_app_should_run_storage_stage(const WatchAppStageTelemetry *telemetry, uint32_t loop_counter);

WdtCheckpointResult watch_app_result_input_stage(void);
WdtCheckpointResult watch_app_result_sensor_stage(const SensorSnapshot *snap);
WdtCheckpointResult watch_app_result_model_stage(void);
WdtCheckpointResult watch_app_result_diag_stage(void);
WdtCheckpointResult watch_app_result_storage_stage(void);

void watch_app_apply_model_runtime_requests(uint8_t *last_sensor_sensitivity);
void watch_app_apply_ui_actions(WatchAppSleepRequestState *sleep_request, uint8_t *last_sensor_sensitivity);
void watch_app_after_storage_tick(WatchAppSleepRequestState *sleep_request, uint32_t *last_storage_commit_ms);

/**
 * @brief Initialize runtime-owned orchestration state and dependent services.
 *
 * @param[out] state Runtime-owned watch application state container.
 * @return void
 * @throws None.
 * @boundary_behavior Returns immediately when state is NULL.
 */
/**
 * @brief Initialize runtime-owned orchestration state and dependent services and capture per-stage status.
 *
 * @param[out] state Runtime-owned watch application state container. Must not be NULL.
 * @param[out] out Optional destination for the retained initialization report.
 * @return true when every required runtime stage completed successfully or in an allowed degraded mode; false when a required stage failed or @p state is NULL.
 * @throws None.
 * @boundary_behavior Returns false immediately when @p state is NULL. On failure, @p out receives the last completed stage and the failed stage when provided.
 */
bool watch_app_runtime_init_state_checked(WatchAppRuntimeState *state, WatchAppInitReport *out);

/**
 * @brief Compatibility wrapper for runtime initialization when the caller does not need the retained report.
 *
 * @param[out] state Runtime-owned watch application state container.
 * @return void
 * @throws None.
 * @boundary_behavior Returns immediately when @p state is NULL; initialization failures remain available via watch_app_get_init_report().
 */
void watch_app_runtime_init_state(WatchAppRuntimeState *state);

void watch_app_register_qos_providers(WatchAppRuntimeState *state);

/**
 * @brief Execute one cooperative main-loop iteration using explicit runtime state.
 *
 * @param[in,out] state Runtime-owned watch application state container.
 * @return void
 * @throws None.
 * @boundary_behavior Returns immediately when state is NULL.
 */
void watch_app_runtime_run_loop(WatchAppRuntimeState *state);

const char *watch_app_runtime_stage_name(WatchAppStageId id);
bool watch_app_runtime_get_stage_telemetry(const WatchAppRuntimeState *state,
                                           WatchAppStageId id,
                                           WatchAppStageTelemetry *out);
uint8_t watch_app_runtime_get_stage_history_count(const WatchAppRuntimeState *state);
bool watch_app_runtime_get_stage_history_recent(const WatchAppRuntimeState *state,
                                                uint8_t reverse_index,
                                                WatchAppStageHistoryEntry *out);

#endif

