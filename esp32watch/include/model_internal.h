#ifndef MODEL_INTERNAL_H
#define MODEL_INTERNAL_H

#include "model.h"

/* Split-state snapshots remain authoritative; only read-side compatibility views are synthesized on demand. */
extern ModelDomainState g_model_domain_state;
extern ModelRuntimeState g_model_runtime_state;
extern ModelUiState g_model_ui_state;

/**
 * @brief Commit a runtime-state mutation and normalize compatibility-facing transient state.
 *
 * Compatibility snapshots are synthesized on demand, so commit now means
 * "repair any dependent split-state mirrors before readers snapshot" rather
 * than "mark legacy projections dirty".
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_internal_commit_runtime_mutation(void);

/**
 * @brief Commit a domain-state mutation and normalize compatibility-facing dependent views.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_internal_commit_domain_mutation(void);

/**
 * @brief Commit a UI-state mutation and normalize compatibility-facing dependent views.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_internal_commit_ui_mutation(void);

/**
 * @brief Normalize authoritative split-state before callers snapshot read-side views.
 *
 * Compatibility snapshots are synthesized on demand, so flush now only repairs
 * derived split-state fields such as selected-alarm and popup mirrors.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void model_internal_flush_read_models(void);

/**
 * @brief Build a read-only compatibility snapshot from the authoritative split-state domains.
 *
 * @param[out] out Destination aggregate snapshot. NULL is ignored.
 * @return Pointer to @p out when populated; NULL when @p out is NULL.
 * @throws None.
 */
const WatchModel *model_internal_build_compat_snapshot(WatchModel *out);

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
