#include "watch_app_internal.h"
#include "app_tuning.h"
#include "model.h"
#include "ui_internal.h"
#include "ui_page_registry.h"
#include "services/input_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include "platform_api.h"
#include <string.h>
#include <stddef.h>

static void watch_app_push_stage_history(WatchAppStageId stage,
                                         uint32_t end_ms,
                                         uint32_t duration_ms,
                                         bool over_budget,
                                         bool deferred,
                                         WatchAppStageTelemetry *telemetry,
                                         WatchAppStageHistoryEntry *history,
                                         uint8_t *head,
                                         uint8_t *count,
                                         uint32_t loop_counter)
{
    WatchAppStageHistoryEntry *entry;

    if (telemetry == NULL || history == NULL || head == NULL || count == NULL || stage >= WATCH_APP_STAGE_COUNT) {
        return;
    }

    entry = &history[*head];
    memset(entry, 0, sizeof(*entry));
    entry->timestamp_ms = end_ms;
    entry->duration_ms = (uint16_t)(duration_ms > 0xFFFFU ? 0xFFFFU : duration_ms);
    entry->budget_ms = (uint16_t)(telemetry[stage].budget_ms > 0xFFFFU ? 0xFFFFU : telemetry[stage].budget_ms);
    entry->loop_counter = loop_counter;
    entry->over_budget = over_budget ? 1U : 0U;
    entry->deferred = deferred ? 1U : 0U;
    entry->stage = stage;

    *head = (uint8_t)((*head + 1U) % WATCH_APP_STAGE_HISTORY_CAPACITY);
    if (*count < WATCH_APP_STAGE_HISTORY_CAPACITY) {
        (*count)++;
    }
}

static bool watch_app_stage_is_overloaded(const WatchAppStageTelemetry *telemetry,
                                          WatchAppStageId stage,
                                          uint16_t threshold)
{
    if (telemetry == NULL || stage >= WATCH_APP_STAGE_COUNT) {
        return false;
    }
    return telemetry[stage].consecutive_over_budget >= threshold;
}

void watch_app_reset_stage_telemetry(WatchAppStageTelemetry *telemetry,
                                     WatchAppStageHistoryEntry *history,
                                     uint8_t *head,
                                     uint8_t *count)
{
    size_t i;
    const AppTuningManifest *tuning = app_tuning_manifest_get();

    if (telemetry == NULL || history == NULL || head == NULL || count == NULL) {
        return;
    }

    memset(telemetry, 0, sizeof(WatchAppStageTelemetry) * WATCH_APP_STAGE_COUNT);
    memset(history, 0, sizeof(WatchAppStageHistoryEntry) * WATCH_APP_STAGE_HISTORY_CAPACITY);
    *head = 0U;
    *count = 0U;
    for (i = 0U; i < WATCH_APP_STAGE_COUNT; ++i) {
        telemetry[i].budget_ms = tuning->watch_app_stage_budget_ms[i];
    }
}

void watch_app_note_stage_deferred(WatchAppStageId stage,
                                   WatchAppStageTelemetry *telemetry,
                                   WatchAppStageHistoryEntry *history,
                                   uint8_t *head,
                                   uint8_t *count,
                                   uint32_t loop_counter)
{
    if (telemetry == NULL || stage >= WATCH_APP_STAGE_COUNT) {
        return;
    }
    if (telemetry[stage].deferred_count < 0xFFFFU) {
        telemetry[stage].deferred_count++;
    }
    watch_app_push_stage_history(stage, watch_app_stage_clock_now(), 0U, false, true, telemetry, history, head, count, loop_counter);
}

void watch_app_record_stage_timing(WatchAppStageId stage,
                                   uint32_t start_ms,
                                   uint32_t end_ms,
                                   WatchAppStageTelemetry *telemetry,
                                   WatchAppStageHistoryEntry *history,
                                   uint8_t *head,
                                   uint8_t *count,
                                   uint32_t loop_counter)
{
    WatchAppStageTelemetry *stage_telemetry;
    uint32_t duration;

    if (telemetry == NULL || stage >= WATCH_APP_STAGE_COUNT) {
        return;
    }

    stage_telemetry = &telemetry[stage];
    duration = end_ms - start_ms;
    stage_telemetry->last_duration_ms = duration;
    stage_telemetry->total_duration_ms += duration;
    stage_telemetry->sample_count++;
    if (duration > stage_telemetry->max_duration_ms) {
        stage_telemetry->max_duration_ms = duration;
    }
    if (duration > stage_telemetry->budget_ms) {
        if (stage_telemetry->over_budget_count < 0xFFFFU) {
            stage_telemetry->over_budget_count++;
        }
        if (stage_telemetry->consecutive_over_budget < 0xFFFFU) {
            stage_telemetry->consecutive_over_budget++;
        }
        if (stage_telemetry->consecutive_over_budget > stage_telemetry->max_consecutive_over_budget) {
            stage_telemetry->max_consecutive_over_budget = stage_telemetry->consecutive_over_budget;
        }
        watch_app_push_stage_history(stage, end_ms, duration, true, false, telemetry, history, head, count, loop_counter);
    } else {
        stage_telemetry->consecutive_over_budget = 0U;
    }
}

bool watch_app_should_defer_render(const WatchAppStageTelemetry *telemetry, uint32_t loop_counter)
{
    const UiPageOps *ops;
    ModelUiState ui_state;

    if (!watch_app_stage_is_overloaded(telemetry, WATCH_APP_STAGE_RENDER, 2U)) {
        return false;
    }
    UiRuntimeSnapshot ui_runtime;

    if (ui_get_runtime_snapshot(&ui_runtime) && ui_runtime.animating) {
        return false;
    }
    if (model_get_ui_state(&ui_state) != NULL && ui_state.popup != POPUP_NONE) {
        return false;
    }
    ops = ui_page_registry_get((ui_get_runtime_snapshot(&ui_runtime) ? ui_runtime.current : PAGE_WATCHFACE));
    if (ops->refresh_policy == UI_REFRESH_TIMER || ops->refresh_policy == UI_REFRESH_STOPWATCH) {
        return false;
    }
    return (loop_counter & 1U) != 0U;
}

bool watch_app_should_run_diag_stage(const WatchAppStageTelemetry *telemetry, uint32_t loop_counter)
{
    if (!watch_app_stage_is_overloaded(telemetry, WATCH_APP_STAGE_DIAG, 2U)) {
        return true;
    }
    if (diag_service_is_safe_mode_active()) {
        return true;
    }
    return (loop_counter & 1U) == 0U;
}

bool watch_app_should_run_storage_stage(const WatchAppStageTelemetry *telemetry, uint32_t loop_counter)
{
    StorageCommitState commit_state;

    if (!watch_app_stage_is_overloaded(telemetry, WATCH_APP_STAGE_STORAGE, 2U)) {
        return true;
    }
    if (!storage_service_has_pending()) {
        return true;
    }
    if (storage_service_is_transaction_active() || storage_service_is_sleep_flush_pending()) {
        return true;
    }
    commit_state = storage_service_get_commit_state();
    if (commit_state != STORAGE_COMMIT_STATE_STAGED) {
        return true;
    }
    return (loop_counter & 1U) == 0U;
}

WdtCheckpointResult watch_app_result_input_stage(void)
{
    return input_service_get_overflow_count() == 0U ? WDT_CHECKPOINT_RESULT_OK : WDT_CHECKPOINT_RESULT_DEGRADED;
}

WdtCheckpointResult watch_app_result_sensor_stage(const SensorSnapshot *snap)
{
    if (snap == NULL || !snap->online) {
        return WDT_CHECKPOINT_RESULT_FAILED;
    }
    if (!snap->calibrated || snap->quality < 40U) {
        return WDT_CHECKPOINT_RESULT_DEGRADED;
    }
    return WDT_CHECKPOINT_RESULT_OK;
}

WdtCheckpointResult watch_app_result_model_stage(void)
{
    ModelDomainState domain_state;
    ModelUiState ui_state;

    if (model_get_domain_state(&domain_state) == NULL || model_get_ui_state(&ui_state) == NULL) {
        return WDT_CHECKPOINT_RESULT_FAILED;
    }
    if (!domain_state.time_valid || ui_state.popup == POPUP_SENSOR_FAULT) {
        return WDT_CHECKPOINT_RESULT_DEGRADED;
    }
    return WDT_CHECKPOINT_RESULT_OK;
}

WdtCheckpointResult watch_app_result_diag_stage(void)
{
    DiagFaultCode code;
    DiagFaultInfo info;

    if (diag_service_is_safe_mode_active()) {
        return WDT_CHECKPOINT_RESULT_FAILED;
    }
    if (diag_service_get_last_fault(&code, &info)) {
        (void)code;
        return info.severity == DIAG_FAULT_SEVERITY_RECOVERABLE ?
            WDT_CHECKPOINT_RESULT_DEGRADED :
            (info.severity == DIAG_FAULT_SEVERITY_NONE ? WDT_CHECKPOINT_RESULT_OK : WDT_CHECKPOINT_RESULT_DEGRADED);
    }
    return WDT_CHECKPOINT_RESULT_OK;
}

WdtCheckpointResult watch_app_result_storage_stage(void)
{
    ModelRuntimeState runtime_state;

    if (model_get_runtime_state(&runtime_state) == NULL) {
        return WDT_CHECKPOINT_RESULT_FAILED;
    }
    if (!runtime_state.storage_ok || !runtime_state.storage_crc_ok) {
        return WDT_CHECKPOINT_RESULT_FAILED;
    }
    if (storage_service_has_pending() ||
        storage_service_is_transaction_active() ||
        storage_service_is_sleep_flush_pending()) {
        return WDT_CHECKPOINT_RESULT_DEGRADED;
    }
    return WDT_CHECKPOINT_RESULT_OK;
}
