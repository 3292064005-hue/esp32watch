#ifndef MODEL_INTERNAL_H
#define MODEL_INTERNAL_H

#include "model.h"

/* Read-only compatibility projection rebuilt from split-state snapshots. */
extern WatchModel g_model;
extern ModelDomainState g_model_domain_state;
extern ModelRuntimeState g_model_runtime_state;
extern ModelUiState g_model_ui_state;

typedef enum {
    MODEL_PROJECTION_DIRTY_NONE = 0,
    MODEL_PROJECTION_DIRTY_DOMAIN = 1 << 0,
    MODEL_PROJECTION_DIRTY_RUNTIME = 1 << 1,
    MODEL_PROJECTION_DIRTY_UI = 1 << 2,
    MODEL_PROJECTION_DIRTY_ALL = (MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_RUNTIME | MODEL_PROJECTION_DIRTY_UI)
} ModelProjectionDirtyFlags;

/**
/**
 * @brief Rebuild the read-only legacy projection after an attempted legacy mutation.
 *
 * The authoritative build no longer accepts write-side mutations through WatchModel.
 * These helpers discard direct g_model edits and restore the projection from the
 * split-state authority without writing anything back into the canonical domains.
 */
void model_internal_commit_legacy_mutation(void);
void model_internal_commit_legacy_mutation_with_flags(uint32_t flags);

/**
 * @brief Commit a runtime-state mutation and refresh the legacy compatibility projection.
 *
 * @param[in] flags Bitwise OR of ModelProjectionDirtyFlags values that changed alongside the runtime mutation.
 * @return void
 * @throws None.
 * @boundary_behavior Ignores MODEL_PROJECTION_DIRTY_NONE and rebuilds the affected read-only projection slices from ModelRuntimeState before returning.
 */
void model_internal_commit_runtime_mutation_with_flags(uint32_t flags);
void model_internal_commit_runtime_mutation(void);

/**
 * @brief Commit a domain-state mutation and refresh the legacy compatibility projection.
 *
 * @param[in] flags Bitwise OR of ModelProjectionDirtyFlags values that changed alongside the domain mutation.
 * @return void
 * @throws None.
 */
void model_internal_commit_domain_mutation_with_flags(uint32_t flags);
void model_internal_commit_domain_mutation(void);

/**
 * @brief Commit a UI-state mutation and refresh the legacy compatibility projection.
 *
 * @param[in] flags Bitwise OR of ModelProjectionDirtyFlags values that changed alongside the UI mutation.
 * @return void
 * @throws None.
 */
void model_internal_commit_ui_mutation_with_flags(uint32_t flags);
void model_internal_commit_ui_mutation(void);

/**
 * @brief Discard accidental legacy aggregate edits and rebuild the projection.
 *
 * @note The split-state domains remain authoritative. These helpers exist only
 *       for compatibility during the projection retirement window.
 */
void model_internal_sync_split_state_from_legacy(void);
void model_internal_sync_legacy_from_split_state(void);

/**
 * @brief Mark one or more read-side model projections dirty after a write-side mutation.
 *
 * @param[in] flags Bitwise OR of ModelProjectionDirtyFlags values.
 * @return void
 * @throws None.
 * @boundary_behavior Ignores MODEL_PROJECTION_DIRTY_NONE.
 */
void model_internal_mark_projection_dirty(uint32_t flags);

/**
 * @brief Flush pending read-side projections from the legacy write model into split snapshots.
 *
 * @param None.
 * @return void
 * @throws None.
 * @boundary_behavior Returns immediately when no projection is dirty.
 */
void model_internal_flush_read_models(void);

/**
 * @brief Stage settings domain data for the runtime orchestrator.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_internal_persist_settings(void);

/**
 * @brief Stage alarm domain data for the runtime orchestrator.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_internal_persist_all_alarms(void);
void model_internal_persist_game_stats(void);

/**
 * @brief Request a runtime side-effect bundle to be executed by the app loop.
 *
 * @param[in] flags Bitwise OR of ModelRuntimeRequestFlags values.
 * @param[in] reason Storage commit reason associated with the request bundle.
 * @return void
 * @throws None.
 */
void model_internal_request_runtime_sync(uint32_t flags, StorageCommitReason reason);

void model_internal_sync_selected_alarm_view(void);
uint32_t model_internal_timeout_idx_to_ms(uint8_t idx);
void model_internal_clear_popups(void);
void model_internal_pop_popup(void);

/**
 * @brief Apply battery observability fields through the centralized runtime-state path.
 *
 * @param[in] mv Battery voltage in millivolts.
 * @param[in] percent Battery percentage, clamped to 0..100.
 * @param[in] charging Whether charging is active.
 * @param[in] present Whether battery telemetry is currently valid.
 * @return void
 * @throws None.
 */
void model_runtime_apply_battery_snapshot(uint16_t mv, uint8_t percent, bool charging, bool present);

/**
 * @brief Apply sensor observability fields through the centralized runtime-state path.
 *
 * @param[in] snap Latest sensor snapshot. NULL is ignored.
 * @return void
 * @throws None.
 */
void model_runtime_apply_sensor_snapshot(const SensorSnapshot *snap);

/**
 * @brief Refresh storage observability fields from the active storage service.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_runtime_apply_storage_observability(void);

/**
 * @brief Refresh power observability fields from the active power service.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_runtime_apply_power_observability(void);

/**
 * @brief Refresh input observability fields from the active input service.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_runtime_apply_input_observability(void);

/**
 * @brief Record generic user activity timestamps through the centralized runtime-state path.
 *
 * @param[in] now_ms Activity timestamp in milliseconds.
 * @return void
 * @throws None.
 */
void model_runtime_note_user_activity(uint32_t now_ms);

/**
 * @brief Record the last completed render timestamp.
 *
 * @param[in] now_ms Render completion timestamp in milliseconds.
 * @return void
 * @throws None.
 */
void model_runtime_note_render(uint32_t now_ms);

/**
 * @brief Record a manual wake event and refresh the paired user-activity timestamp.
 *
 * @param[in] now_ms Wake timestamp in milliseconds.
 * @return void
 * @throws None.
 */
void model_runtime_note_manual_wake(uint32_t now_ms);

void model_internal_sync_storage_runtime(void);
void model_internal_sync_power_runtime(void);
void model_internal_sync_input_runtime(void);

/**
 * @brief Trigger an alarm instance and stage runtime persistence side effects.
 *
 * @param[in] index Alarm index to activate. Values outside the valid range are ignored.
 * @param[in] due_day_id Day identifier captured at trigger time for repeat suppression.
 * @return void
 * @throws None.
 */
void model_trigger_alarm(uint8_t index, uint16_t due_day_id);

#endif
