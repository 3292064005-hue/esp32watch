#include "services/storage_service_internal.h"
#include "app_tuning.h"
#include "crash_capsule.h"
#include <stddef.h>

static uint8_t storage_commit_reason_priority(StorageCommitReason reason)
{
    switch (reason) {
        case STORAGE_COMMIT_REASON_SLEEP: return 5U;
        case STORAGE_COMMIT_REASON_RESTORE_DEFAULTS: return 4U;
        case STORAGE_COMMIT_REASON_ALARM: return 3U;
        case STORAGE_COMMIT_REASON_MANUAL:
        case STORAGE_COMMIT_REASON_CALIBRATION: return 2U;
        case STORAGE_COMMIT_REASON_MAX_AGE:
        case STORAGE_COMMIT_REASON_IDLE: return 1U;
        default: return 0U;
    }
}

uint8_t storage_scheduler_pending_mask(const StorageServiceState *state)
{
    uint8_t mask = 0U;

    if (state == NULL) {
        return 0U;
    }
    if (state->pending_settings) {
        mask |= STORAGE_PENDING_SETTINGS;
    }
    if (state->pending_alarms) {
        mask |= STORAGE_PENDING_ALARMS;
    }
    if (state->pending_calibration) {
        mask |= STORAGE_PENDING_CALIBRATION;
    }
    if (state->pending_game_stats) {
        mask |= STORAGE_PENDING_GAME_STATS;
    }
    return mask;
}

bool storage_scheduler_has_pending(const StorageServiceState *state)
{
    return storage_scheduler_pending_mask(state) != 0U;
}

void storage_scheduler_mark_dirty(StorageServiceState *state, uint32_t now_ms, uint8_t source_bit)
{
    if (state == NULL) {
        return;
    }
    if (state->dirty_since_ms == 0U) {
        state->dirty_since_ms = now_ms;
    }
    state->last_change_ms = now_ms;
    state->dirty_source_mask |= source_bit;
    crash_capsule_note_storage(state->commit_count, storage_scheduler_pending_mask(state));
}

void storage_scheduler_remember_requested_reason(StorageServiceState *state, StorageCommitReason reason)
{
    if (state == NULL || reason == STORAGE_COMMIT_REASON_NONE) {
        return;
    }
    if (storage_commit_reason_priority(reason) >= storage_commit_reason_priority(state->requested_commit_reason)) {
        state->requested_commit_reason = reason;
    }
}

StorageCommitReason storage_scheduler_choose_due_reason(const StorageServiceState *state, uint32_t now_ms)
{
    bool due_idle;
    bool due_max;
    bool due_cal;

    if (state == NULL || !storage_scheduler_has_pending(state) || state->transaction_active || state->commit_in_progress) {
        return STORAGE_COMMIT_REASON_NONE;
    }
    if (state->flush_before_sleep) {
        return STORAGE_COMMIT_REASON_SLEEP;
    }
    if (state->requested_commit_reason != STORAGE_COMMIT_REASON_NONE) {
        return state->requested_commit_reason;
    }

    due_idle = (now_ms - state->last_change_ms) >= app_tuning_manifest_get()->storage_commit_idle_ms;
    due_max = (now_ms - state->dirty_since_ms) >= app_tuning_manifest_get()->storage_commit_max_ms;
    due_cal = state->pending_calibration &&
              ((now_ms - state->last_change_ms) >= app_tuning_manifest_get()->storage_commit_cal_ms);

    if (due_cal) {
        return STORAGE_COMMIT_REASON_CALIBRATION;
    }
    if (due_max) {
        return STORAGE_COMMIT_REASON_MAX_AGE;
    }
    if (due_idle) {
        return STORAGE_COMMIT_REASON_IDLE;
    }
    return STORAGE_COMMIT_REASON_NONE;
}

StorageCommitState storage_scheduler_commit_state(const StorageServiceState *state)
{
    if (state == NULL) {
        return STORAGE_COMMIT_STATE_IDLE;
    }
    if (state->transaction_active) {
        return STORAGE_COMMIT_STATE_TRANSACTION;
    }
    if (!storage_scheduler_has_pending(state)) {
        return STORAGE_COMMIT_STATE_IDLE;
    }
    if (state->flush_before_sleep) {
        return STORAGE_COMMIT_STATE_FLUSH_BEFORE_SLEEP;
    }
    if (state->requested_commit_reason != STORAGE_COMMIT_REASON_NONE) {
        return STORAGE_COMMIT_STATE_REQUESTED;
    }
    return STORAGE_COMMIT_STATE_STAGED;
}
