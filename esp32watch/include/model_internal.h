#ifndef MODEL_INTERNAL_H
#define MODEL_INTERNAL_H

#include "model.h"

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
 * @brief Commit a legacy write-side model mutation and rebuild all read snapshots.
 *
 * @param None.
 * @return void
 * @throws None.
 * @note This is the preferred post-mutation boundary for write-side model code.
 *       It reconciles legacy mirror fields before rebuilding the split read models.
 */
void model_internal_commit_legacy_mutation(void);
void model_internal_commit_legacy_mutation_with_flags(uint32_t flags);
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
