#include "services/storage_service.h"
#include "services/storage_service_internal.h"
#include "app_config.h"
#include "crash_capsule.h"
#include "persist.h"
#include "persist_flash.h"
#include "services/diag_service.h"
#include "platform_api.h"
#include <string.h>

StorageServiceState g_storage;

typedef enum {
    STORAGE_COMMIT_EVENT_BEGIN = 0,
    STORAGE_COMMIT_EVENT_BACKEND_BEGIN_OK,
    STORAGE_COMMIT_EVENT_BACKEND_BEGIN_FAIL,
    STORAGE_COMMIT_EVENT_BACKEND_WRITE_DONE,
    STORAGE_COMMIT_EVENT_BACKEND_WRITE_FAIL,
    STORAGE_COMMIT_EVENT_FINALIZE,
    STORAGE_COMMIT_EVENT_ABORT
} StorageCommitEvent;

static bool storage_commit_next_phase(StorageCommitPhase from, StorageCommitEvent event, StorageCommitPhase *out_phase)
{
    if (out_phase == NULL) {
        return false;
    }

    switch (from) {
        case STORAGE_COMMIT_PHASE_IDLE:
            if (event == STORAGE_COMMIT_EVENT_BEGIN) {
                *out_phase = STORAGE_COMMIT_PHASE_PREPARE;
                return true;
            }
            break;
        case STORAGE_COMMIT_PHASE_PREPARE:
            if (event == STORAGE_COMMIT_EVENT_BACKEND_BEGIN_OK) {
                *out_phase = STORAGE_COMMIT_PHASE_WRITE;
                return true;
            }
            if (event == STORAGE_COMMIT_EVENT_BACKEND_BEGIN_FAIL || event == STORAGE_COMMIT_EVENT_ABORT) {
                *out_phase = STORAGE_COMMIT_PHASE_FINALIZE;
                return true;
            }
            break;
        case STORAGE_COMMIT_PHASE_WRITE:
            if (event == STORAGE_COMMIT_EVENT_BACKEND_WRITE_DONE ||
                event == STORAGE_COMMIT_EVENT_BACKEND_WRITE_FAIL ||
                event == STORAGE_COMMIT_EVENT_ABORT) {
                *out_phase = STORAGE_COMMIT_PHASE_FINALIZE;
                return true;
            }
            break;
        case STORAGE_COMMIT_PHASE_FINALIZE:
            if (event == STORAGE_COMMIT_EVENT_FINALIZE) {
                *out_phase = STORAGE_COMMIT_PHASE_IDLE;
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

static void storage_commit_apply_event(StorageCommitEvent event)
{
    StorageCommitPhase next_phase;

    if (!storage_commit_next_phase(g_storage.commit_phase, event, &next_phase)) {
        /*
         * Collapse illegal phase transitions without recursive re-entry. The
         * guard itself is part of the failure path, so it must not depend on a
         * second transition check succeeding from a corrupted current phase.
         */
        g_storage.backend_degraded = true;
        g_storage.verify_failed = true;
        g_storage.last_commit_ok = false;
        g_storage.last_commit_ms = platform_time_now_ms();
        g_storage.last_commit_reason = g_storage.inflight_commit_reason;
        g_storage.inflight_commit_ok = false;
        storage_backend_adapter_abort_commit();
        g_storage.commit_phase = STORAGE_COMMIT_PHASE_FINALIZE;
        return;
    }

    g_storage.commit_phase = next_phase;
}

static void storage_commit_prepare_payloads(SettingsState *settings_to_write,
                                            AlarmState *alarms_to_write,
                                            SensorCalibrationData *cal_to_write)
{
    if (g_storage.pending_settings) {
        *settings_to_write = g_storage.settings;
    } else if (g_storage.shadow_settings_valid) {
        *settings_to_write = g_storage.shadow_settings;
    } else {
        storage_backend_adapter_load_settings(settings_to_write);
    }

    if (g_storage.pending_alarms) {
        memcpy(alarms_to_write, g_storage.alarms, sizeof(g_storage.alarms));
    } else if (g_storage.shadow_alarms_valid) {
        memcpy(alarms_to_write, g_storage.shadow_alarms, sizeof(g_storage.shadow_alarms));
    } else {
        storage_backend_adapter_load_alarms(alarms_to_write, APP_MAX_ALARMS);
    }

    if (g_storage.pending_calibration) {
        *cal_to_write = g_storage.calibration;
    } else if (g_storage.shadow_calibration_valid) {
        *cal_to_write = g_storage.shadow_calibration;
    } else {
        storage_backend_adapter_load_calibration(cal_to_write);
    }
}

static void storage_commit_apply_success(const SettingsState *settings_to_write,
                                         const AlarmState *alarms_to_write,
                                         const SensorCalibrationData *cal_to_write)
{
    g_storage.requested_commit_reason = STORAGE_COMMIT_REASON_NONE;
    g_storage.flush_before_sleep = false;
    g_storage.shadow_settings = *settings_to_write;
    memcpy(g_storage.shadow_alarms, alarms_to_write, sizeof(g_storage.shadow_alarms));
    g_storage.shadow_calibration = *cal_to_write;
    g_storage.shadow_settings_valid = true;
    g_storage.shadow_alarms_valid = true;
    g_storage.shadow_calibration_valid = true;
    g_storage.pending_settings = false;
    g_storage.pending_alarms = false;
    g_storage.pending_calibration = false;
    g_storage.dirty_since_ms = 0U;
    g_storage.last_change_ms = 0U;
    g_storage.dirty_source_mask = 0U;
    storage_backend_adapter_capture_shadows(&g_storage);
    g_storage.commit_count++;
    g_storage.last_commit_ok = storage_recovery_verify_runtime_state(&g_storage);
}

static bool storage_commit_prepare_execution(StorageCommitReason reason)
{
    StorageCommitExecutionContext tx_ctx;

    if (!storage_tx_prepare_commit(&g_storage, reason, &tx_ctx)) {
        return false;
    }

    storage_commit_prepare_payloads(&g_storage.inflight_settings,
                                    g_storage.inflight_alarms,
                                    &g_storage.inflight_calibration);
    g_storage.inflight_commit_reason = reason;
    g_storage.inflight_commit_ok = false;
    g_storage.inflight_previously_transaction_active = tx_ctx.previously_transaction_active;
    storage_commit_apply_event(STORAGE_COMMIT_EVENT_BEGIN);
    return true;
}

static void storage_commit_execute_prepared(void)
{
    StorageBackendCommitTickResult result;

    if (g_storage.commit_phase == STORAGE_COMMIT_PHASE_PREPARE) {
        if (!storage_backend_adapter_begin_commit(&g_storage.inflight_settings,
                                                  g_storage.inflight_alarms,
                                                  APP_MAX_ALARMS,
                                                  &g_storage.inflight_calibration)) {
            g_storage.last_commit_ms = platform_time_now_ms();
            g_storage.last_commit_reason = g_storage.inflight_commit_reason;
            g_storage.last_commit_ok = false;
            g_storage.inflight_commit_ok = false;
            g_storage.backend_degraded = true;
            g_storage.verify_failed = true;
            g_storage.flush_before_sleep = false;
            g_storage.requested_commit_reason = STORAGE_COMMIT_REASON_NONE;
            storage_commit_apply_event(STORAGE_COMMIT_EVENT_BACKEND_BEGIN_FAIL);
            return;
        }
        storage_commit_apply_event(STORAGE_COMMIT_EVENT_BACKEND_BEGIN_OK);
        return;
    }

    result = storage_backend_adapter_commit_tick();
    if (result == STORAGE_BACKEND_COMMIT_TICK_IN_PROGRESS) {
        return;
    }

    g_storage.last_commit_ms = platform_time_now_ms();
    g_storage.last_commit_reason = g_storage.inflight_commit_reason;
    g_storage.inflight_commit_ok = result == STORAGE_BACKEND_COMMIT_TICK_DONE_OK;

    if (g_storage.inflight_commit_ok) {
        storage_commit_apply_success(&g_storage.inflight_settings,
                                     g_storage.inflight_alarms,
                                     &g_storage.inflight_calibration);
    } else {
        g_storage.last_commit_ok = false;
        g_storage.backend_degraded = true;
        g_storage.verify_failed = true;
        g_storage.flush_before_sleep = false;
        g_storage.requested_commit_reason = STORAGE_COMMIT_REASON_NONE;
        storage_backend_adapter_abort_commit();
    }

    storage_commit_apply_event(g_storage.inflight_commit_ok ? STORAGE_COMMIT_EVENT_BACKEND_WRITE_DONE : STORAGE_COMMIT_EVENT_BACKEND_WRITE_FAIL);
}

static void storage_commit_finalize_execution(void)
{
    StorageCommitExecutionContext tx_ctx = {0};

    tx_ctx.previously_transaction_active = g_storage.inflight_previously_transaction_active;
    storage_tx_finish_commit(&g_storage, &tx_ctx, g_storage.inflight_commit_ok);
    crash_capsule_note_storage(g_storage.commit_count, storage_scheduler_pending_mask(&g_storage));
    diag_service_note_storage_commit(g_storage.inflight_commit_reason, g_storage.last_commit_ok, g_storage.commit_count);
    g_storage.inflight_commit_reason = STORAGE_COMMIT_REASON_NONE;
    g_storage.inflight_commit_ok = false;
    storage_commit_apply_event(STORAGE_COMMIT_EVENT_FINALIZE);
}

/**
 * @brief Initialize the storage service and capture backend shadow state.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void storage_service_init(void)
{
    memset(&g_storage, 0, sizeof(g_storage));
    persist_init();
    persist_flash_init();
    (void)storage_backend_adapter_migrate_from_bkp_to_flash();
    g_storage.last_commit_reason = STORAGE_COMMIT_REASON_NONE;
    g_storage.last_commit_ok = false;
    storage_backend_adapter_capture_shadows(&g_storage);
    g_storage.last_commit_ok = storage_backend_adapter_is_initialized();
    g_storage.backend_degraded = !storage_backend_adapter_is_initialized();
    g_storage.verify_failed = !storage_recovery_verify_runtime_state(&g_storage);
    crash_capsule_note_storage(g_storage.commit_count, storage_scheduler_pending_mask(&g_storage));
}

/**
 * @brief Advance storage commit scheduling and run due commits.
 *
 * @param[in] now_ms Monotonic system tick in milliseconds.
 * @return void
 * @throws None.
 */
void storage_service_tick(uint32_t now_ms)
{
    StorageCommitReason due_reason;

    if (g_storage.commit_phase == STORAGE_COMMIT_PHASE_PREPARE ||
        g_storage.commit_phase == STORAGE_COMMIT_PHASE_WRITE) {
        storage_commit_execute_prepared();
        return;
    }
    if (g_storage.commit_phase == STORAGE_COMMIT_PHASE_FINALIZE) {
        storage_commit_finalize_execution();
        return;
    }

    if (!storage_scheduler_has_pending(&g_storage)) {
        g_storage.flush_before_sleep = false;
        g_storage.requested_commit_reason = STORAGE_COMMIT_REASON_NONE;
        return;
    }

    due_reason = storage_scheduler_choose_due_reason(&g_storage, now_ms);
    if (due_reason != STORAGE_COMMIT_REASON_NONE) {
        (void)storage_commit_prepare_execution(due_reason);
    }
}

void storage_service_begin_transaction(void)
{
    storage_tx_begin(&g_storage);
}

void storage_service_finalize_transaction(bool commit, StorageCommitReason reason)
{
    bool should_commit = g_storage.transaction_active && commit && storage_scheduler_has_pending(&g_storage);

    storage_tx_finalize(&g_storage, commit, reason);
    if (should_commit) {
        storage_service_commit_now_with_reason(reason);
    }
}

void storage_service_abort_transaction(void)
{
    storage_tx_abort(&g_storage);
}

void storage_service_request_commit(StorageCommitReason reason)
{
    if (!storage_scheduler_has_pending(&g_storage)) {
        return;
    }
    storage_scheduler_remember_requested_reason(&g_storage, reason);
}

void storage_service_request_flush_before_sleep(void)
{
    if (!storage_scheduler_has_pending(&g_storage)) {
        g_storage.flush_before_sleep = false;
        return;
    }
    g_storage.flush_before_sleep = true;
    storage_scheduler_remember_requested_reason(&g_storage, STORAGE_COMMIT_REASON_SLEEP);
}

bool storage_service_is_sleep_flush_pending(void)
{
    return g_storage.flush_before_sleep && storage_scheduler_has_pending(&g_storage);
}

void storage_service_commit_now(void)
{
    storage_service_commit_now_with_reason(STORAGE_COMMIT_REASON_MANUAL);
}

/**
 * @brief Force an immediate storage commit while preserving public transaction semantics.
 *
 * @param[in] reason Commit reason recorded in runtime observability and diagnostics.
 * @return void
 * @throws None.
 */
void storage_service_commit_now_with_reason(StorageCommitReason reason)
{
    if (!storage_commit_prepare_execution(reason)) {
        return;
    }

    for (uint8_t guard = 0U; guard < 24U && g_storage.commit_phase != STORAGE_COMMIT_PHASE_IDLE; ++guard) {
        if (g_storage.commit_phase == STORAGE_COMMIT_PHASE_PREPARE ||
            g_storage.commit_phase == STORAGE_COMMIT_PHASE_WRITE) {
            storage_commit_execute_prepared();
        } else if (g_storage.commit_phase == STORAGE_COMMIT_PHASE_FINALIZE) {
            storage_commit_finalize_execution();
        }
    }

    if (g_storage.commit_phase != STORAGE_COMMIT_PHASE_IDLE) {
        storage_backend_adapter_abort_commit();
        g_storage.last_commit_ok = false;
        g_storage.backend_degraded = true;
        g_storage.verify_failed = true;
        g_storage.last_commit_ms = platform_time_now_ms();
        g_storage.last_commit_reason = reason;
        g_storage.inflight_commit_reason = reason;
        g_storage.inflight_commit_ok = false;
        storage_commit_apply_event(STORAGE_COMMIT_EVENT_ABORT);
        storage_commit_finalize_execution();
    }
}


