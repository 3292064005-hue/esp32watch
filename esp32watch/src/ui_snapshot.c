#include "ui_snapshot.h"
#include "app_config.h"
#include "model.h"
#include "ui_system_read_model.h"
#include <string.h>

static uint8_t ui_snapshot_progress_percent(uint32_t part, uint32_t total)
{
    return (uint8_t)APP_CLAMP((total == 0U) ? 0U : (part * 100UL) / total, 0U, 100U);
}

void ui_snapshot_build(UiSystemSnapshot *out, uint32_t now_ms)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    ModelUiState ui_state;
    UiSystemReadModel system_read_model;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    (void)model_get_domain_state(&domain_state);
    (void)model_get_runtime_state(&runtime_state);
    (void)model_get_ui_state(&ui_state);

    out->domain = domain_state;
    out->runtime = runtime_state;
    out->ui = ui_state;
    out->game_stats = domain_state.game_stats;

    out->activity.steps = domain_state.activity.steps;
    out->activity.goal = domain_state.activity.goal;
    out->activity.goal_percent = ui_snapshot_progress_percent(domain_state.activity.steps, domain_state.activity.goal);
    out->activity.activity_level = domain_state.activity.activity_level;
    out->activity.motion_state = domain_state.activity.motion_state;
    out->activity.active_minutes = domain_state.activity.active_minutes;

    ui_system_read_model_capture(&system_read_model, &runtime_state, now_ms);

    out->sensor.online = runtime_state.sensor.online;
    out->sensor.calibrated = runtime_state.sensor.calibrated;
    out->sensor.static_now = runtime_state.sensor.static_now;
    out->sensor.runtime_state = (SensorRuntimeState)runtime_state.sensor.runtime_state;
    out->sensor.calibration_state = (SensorCalibrationState)runtime_state.sensor.calibration_state;
    out->sensor.runtime_state_name = system_read_model.sensor_runtime_state_name;
    out->sensor.calibration_state_name = system_read_model.sensor_calibration_state_name;
    out->sensor.quality = runtime_state.sensor.quality;
    out->sensor.error_code = runtime_state.sensor.error_code;
    out->sensor.calibration_progress = runtime_state.sensor.calibration_progress;
    out->sensor.fault_count = runtime_state.sensor.fault_count;
    out->sensor.reinit_count = runtime_state.sensor.reinit_count;
    out->sensor.ax = runtime_state.sensor.ax;
    out->sensor.ay = runtime_state.sensor.ay;
    out->sensor.az = runtime_state.sensor.az;
    out->sensor.gx = runtime_state.sensor.gx;
    out->sensor.gy = runtime_state.sensor.gy;
    out->sensor.gz = runtime_state.sensor.gz;
    out->sensor.pitch_deg = runtime_state.sensor.pitch_deg;
    out->sensor.roll_deg = runtime_state.sensor.roll_deg;
    out->sensor.motion_score = runtime_state.sensor.motion_score;
    out->sensor.retry_backoff_s = system_read_model.sensor_retry_backoff_s;
    out->sensor.offline_elapsed_s = system_read_model.sensor_offline_elapsed_s;

    out->settings.brightness = domain_state.settings.brightness;
    out->settings.auto_wake = domain_state.settings.auto_wake;
    out->settings.auto_sleep = domain_state.settings.auto_sleep;
    out->settings.dnd = domain_state.settings.dnd;
    out->settings.goal = domain_state.activity.goal;
    out->settings.watchface = domain_state.settings.watchface;
    out->settings.show_seconds = domain_state.settings.show_seconds;
    out->settings.animations = domain_state.settings.animations;
    out->settings.screen_timeout_s = runtime_state.screen_timeout_ms / 1000UL;
    out->settings.sensor_sensitivity = domain_state.settings.sensor_sensitivity;
    out->settings.time_valid = domain_state.time_valid;
    out->settings.current_time = domain_state.now;

    out->storage.version = runtime_state.storage_version;
    out->storage.initialized = runtime_state.storage_ok;
    out->storage.crc_valid = runtime_state.storage_crc_ok;
    out->storage.pending_mask = runtime_state.storage_pending_mask;
    out->storage.dirty_source_mask = runtime_state.storage_dirty_source_mask;
    out->storage.stored_crc = runtime_state.storage_stored_crc;
    out->storage.calculated_crc = runtime_state.storage_calc_crc;
    out->storage.commit_count = runtime_state.storage_commit_count;
    out->storage.last_commit_ok = runtime_state.storage_last_commit_ok != 0U;
    out->storage.last_commit_reason_name = model_storage_commit_reason_name(runtime_state.storage_last_commit_reason);
    out->storage.backend_name = system_read_model.storage_backend_name;
    out->storage.commit_state_name = system_read_model.storage_commit_state_name;
    out->storage.transaction_active = system_read_model.storage_transaction_active;
    out->storage.sleep_flush_pending = system_read_model.storage_sleep_flush_pending;
    out->storage.wear_count = system_read_model.storage_wear_count;

    out->storage_backend_name = out->storage.backend_name;
    out->storage_commit_state_name = out->storage.commit_state_name;
    out->storage_transaction_active = out->storage.transaction_active;
    out->storage_sleep_flush_pending = out->storage.sleep_flush_pending;
    out->storage_wear_count = out->storage.wear_count;

    out->safe_mode_active = system_read_model.safe_mode_active;
    out->safe_mode_can_clear = system_read_model.safe_mode_can_clear;
    out->safe_mode_reason_name = system_read_model.safe_mode_reason_name;
    out->has_last_fault = system_read_model.has_last_fault;
    out->last_fault_name = system_read_model.last_fault_name;
    out->last_fault_severity_name = system_read_model.last_fault_severity_name;
    out->last_fault_count = system_read_model.last_fault_count;
    out->last_fault_value = system_read_model.last_fault_value;
    out->last_fault_aux = system_read_model.last_fault_aux;
    out->retained_checkpoint = system_read_model.retained_checkpoint;
    out->retained_max_loop_ms = system_read_model.retained_max_loop_ms;
    out->reset_reason_name = model_reset_reason_name(runtime_state.reset_reason);
    out->wake_reason_name = model_wake_reason_name(runtime_state.last_wake_reason);
    out->sleep_reason_name = model_sleep_reason_name(runtime_state.last_sleep_reason);
    out->time_state_name = model_time_state_name(domain_state.time_state);
    out->storage_last_commit_reason_name = model_storage_commit_reason_name(runtime_state.storage_last_commit_reason);
    out->wdt_last_checkpoint_name = system_read_model.wdt_last_checkpoint_name;
    out->wdt_last_checkpoint_result_name = system_read_model.wdt_last_checkpoint_result_name;
    out->wdt_max_loop_ms = system_read_model.wdt_max_loop_ms;
    out->has_last_log = system_read_model.has_last_log;
    out->last_log_name = system_read_model.last_log_name;
    out->last_log_value = system_read_model.last_log_value;
    out->last_log_aux = system_read_model.last_log_aux;
    out->display_tx_fail_count = system_read_model.display_tx_fail_count;
    out->ui_mutation_overflow_event_count = system_read_model.ui_mutation_overflow_event_count;
    out->key_up_down = system_read_model.key_up_down;
    out->key_down_down = system_read_model.key_down_down;
    out->key_ok_down = system_read_model.key_ok_down;
    out->key_back_down = system_read_model.key_back_down;
}
