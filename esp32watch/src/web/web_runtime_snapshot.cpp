#include "web/web_runtime_snapshot.h"
#include "web/web_state_bridge_internal.h"
#include "services/sensor_service.h"
#include "services/storage_facade.h"
#include "web/web_overlay.h"
#include "web/web_action_queue.h"
#include "web/web_server.h"
#include "melody_service.h"
#include "platform_api.h"
#include <string.h>
#include <cstdio>

extern "C" {
#include "display.h"
#include "services/diag_service.h"
#include "services/wdt_service.h"
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (dst == nullptr || dst_size == 0U) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static void collect_sensor_detail(WebRuntimeSnapshot *out)
{
    const SensorSnapshot *sensor_snapshot = out->has_sensor_snapshot ? &out->sensor_snapshot : sensor_service_get_snapshot();
    WebStateSensorSnapshot *sensor = &out->detail_state.sensor;

    if (sensor_snapshot == nullptr) {
        copy_text(sensor->runtime_state, sizeof(sensor->runtime_state), "UNAVAILABLE");
        copy_text(sensor->calibration_state, sizeof(sensor->calibration_state), "N/A");
        return;
    }

    sensor->online = sensor_snapshot->online;
    sensor->calibrated = sensor_snapshot->calibrated;
    sensor->static_now = sensor_snapshot->static_now;
    sensor->quality = sensor_snapshot->quality;
    sensor->error_code = sensor_snapshot->error_code;
    sensor->fault_count = sensor_snapshot->fault_count;
    sensor->reinit_count = sensor_snapshot->reinit_count;
    sensor->calibration_progress = sensor_snapshot->calibration_progress;
    copy_text(sensor->runtime_state,
              sizeof(sensor->runtime_state),
              sensor_runtime_state_name(sensor_snapshot->runtime_state));
    copy_text(sensor->calibration_state,
              sizeof(sensor->calibration_state),
              sensor_calibration_state_name(sensor_snapshot->calibration_state));
    sensor->ax = sensor_snapshot->ax;
    sensor->ay = sensor_snapshot->ay;
    sensor->az = sensor_snapshot->az;
    sensor->gx = sensor_snapshot->gx;
    sensor->gy = sensor_snapshot->gy;
    sensor->gz = sensor_snapshot->gz;
    sensor->accel_norm_mg = sensor_snapshot->accel_norm_mg;
    sensor->baseline_mg = sensor_snapshot->baseline_mg;
    sensor->motion_score = sensor_snapshot->motion_score;
    sensor->last_sample_ms = sensor_snapshot->last_sample_ms;
    sensor->steps_total = sensor_snapshot->steps_total;
    sensor->pitch_deg = (float)sensor_snapshot->pitch_deg;
    sensor->roll_deg = (float)sensor_snapshot->roll_deg;
}

static void collect_diag_detail(WebRuntimeSnapshot *out)
{
    DiagFaultCode last_fault_code = DIAG_FAULT_NONE;
    DiagFaultInfo last_fault_info = {};
    DiagLogEntry last_log = {};
    DiagExportSnapshot export_snapshot = {};
    WebStateDiagSnapshot *diag = &out->detail_state.diag;

    diag->safe_mode_can_clear = true;
    copy_text(diag->safe_mode_reason, sizeof(diag->safe_mode_reason), "NONE");
    if (diag_service_is_safe_mode_active()) {
        diag->safe_mode_active = true;
        diag->safe_mode_can_clear = diag_service_can_clear_safe_mode();
        copy_text(diag->safe_mode_reason,
                  sizeof(diag->safe_mode_reason),
                  diag_service_safe_mode_reason_name(diag_service_get_safe_mode_reason()));
    }

    copy_text(diag->last_fault_name, sizeof(diag->last_fault_name), "NONE");
    copy_text(diag->last_fault_severity, sizeof(diag->last_fault_severity), "INFO");
    if (diag_service_get_last_fault(&last_fault_code, &last_fault_info)) {
        diag->has_last_fault = true;
        copy_text(diag->last_fault_name,
                  sizeof(diag->last_fault_name),
                  diag_service_fault_name(last_fault_code));
        copy_text(diag->last_fault_severity,
                  sizeof(diag->last_fault_severity),
                  diag_service_fault_severity_name(last_fault_info.severity));
        diag->last_fault_count = last_fault_info.count;
    }

    diag->has_last_log = diag_service_get_last_log(&last_log);
    copy_text(diag->last_log_name,
              sizeof(diag->last_log_name),
              diag->has_last_log ? diag_service_event_name((DiagEventCode)last_log.code) : "NONE");
    diag->last_log_value = diag->has_last_log ? last_log.value : 0U;
    diag->last_log_aux = diag->has_last_log ? last_log.aux : 0U;
    if (diag_service_export_snapshot(&export_snapshot)) {
        diag->consecutive_incomplete_boots = export_snapshot.consecutive_incomplete_boots;
        diag->boot_count = export_snapshot.boot_count;
        diag->previous_boot_count = export_snapshot.previous_boot_count;
    }
}

static void collect_storage_detail(WebRuntimeSnapshot *out)
{
    StorageFacadeRuntimeSnapshot runtime = {};
    WebStateStorageSnapshot *storage = &out->detail_state.storage;

    if (!storage_facade_get_runtime_snapshot(&runtime)) {
        return;
    }

    copy_text(storage->backend, sizeof(storage->backend), runtime.backend);
    copy_text(storage->backend_phase, sizeof(storage->backend_phase), runtime.backend_phase);
    copy_text(storage->commit_state, sizeof(storage->commit_state), runtime.commit_state);
    storage->schema_version = runtime.schema_version;
    storage->flash_supported = runtime.flash_supported;
    storage->flash_ready = runtime.flash_ready;
    storage->migration_attempted = runtime.migration_attempted;
    storage->migration_ok = runtime.migration_ok;
    storage->transaction_active = runtime.transaction_active;
    storage->sleep_flush_pending = runtime.sleep_flush_pending;
    copy_text(storage->app_state_backend,
              sizeof(storage->app_state_backend),
              runtime.app_state_backend);
    copy_text(storage->device_config_backend,
              sizeof(storage->device_config_backend),
              runtime.device_config_backend);
    copy_text(storage->app_state_durability,
              sizeof(storage->app_state_durability),
              runtime.app_state_durability);
    copy_text(storage->device_config_durability,
              sizeof(storage->device_config_durability),
              runtime.device_config_durability);
    storage->app_state_power_loss_guaranteed = runtime.app_state_power_loss_guaranteed;
    storage->device_config_power_loss_guaranteed = runtime.device_config_power_loss_guaranteed;
    storage->app_state_mixed_durability = runtime.app_state_mixed_durability;
    storage->app_state_reset_domain_object_count = runtime.app_state_reset_domain_object_count;
    storage->app_state_durable_object_count = runtime.app_state_durable_object_count;
}

static void collect_alarm_music_overlay_detail(WebRuntimeSnapshot *out)
{
    const uint32_t now_ms = platform_time_now_ms();
    WebStateAlarmSnapshot *alarm_out = &out->detail_state.alarm;
    WebStateMusicSnapshot *music = &out->detail_state.music;
    WebStateOverlaySnapshot *overlay = &out->detail_state.overlay;

    out->detail_state.display.present_count = display_get_present_count();
    out->detail_state.display.tx_fail_count = display_get_tx_error_count();

    alarm_out->next_alarm_index = model_find_next_enabled_alarm();
    if (out->has_domain_state) {
        for (uint8_t i = 0U; i < APP_MAX_ALARMS; ++i) {
            const AlarmState *alarm = &out->domain_state.alarms[i];
            if (!alarm->enabled) {
                continue;
            }
            alarm_out->next_alarm_index = i;
            alarm_out->enabled = alarm->enabled;
            alarm_out->ringing = alarm->ringing;
            snprintf(alarm_out->next_time, sizeof(alarm_out->next_time), "%02u:%02u", alarm->hour, alarm->minute);
            break;
        }
    } else if (alarm_out->next_alarm_index < APP_MAX_ALARMS) {
        const AlarmState *alarm = model_get_alarm(alarm_out->next_alarm_index);
        alarm_out->enabled = alarm->enabled;
        alarm_out->ringing = alarm->ringing;
        snprintf(alarm_out->next_time, sizeof(alarm_out->next_time), "%02u:%02u", alarm->hour, alarm->minute);
    } else {
        alarm_out->next_alarm_index = 0xFFU;
        copy_text(alarm_out->next_time, sizeof(alarm_out->next_time), "--:--");
    }

    music->available = melody_is_available();
    music->playing = melody_is_playing();
    copy_text(music->state, sizeof(music->state), melody_state_name(melody_get_state()));
    copy_text(music->song, sizeof(music->song), melody_song_ascii_name(melody_get_current_song()));

    copy_text(overlay->text, sizeof(overlay->text), web_overlay_last_text());
    overlay->active = web_overlay_is_active(now_ms);
    overlay->expire_at_ms = web_overlay_expire_at_ms();
}

static void collect_perf_detail(WebRuntimeSnapshot *out)
{
    WatchAppStageTelemetry telemetry = {};
    WatchAppStageHistoryEntry history = {};
    WebStatePerfSnapshot *perf = &out->detail_state.perf;

    perf->loop_count = wdt_service_get_loop_count();
    perf->max_loop_ms = wdt_service_get_max_loop_ms();
    perf->action_queue_depth = web_action_queue_depth();
    perf->action_queue_drop_count = web_action_queue_drop_count();
    copy_text(perf->last_checkpoint,
              sizeof(perf->last_checkpoint),
              wdt_service_checkpoint_name(wdt_service_get_last_checkpoint()));
    copy_text(perf->last_checkpoint_result,
              sizeof(perf->last_checkpoint_result),
              wdt_service_checkpoint_result_name(wdt_service_get_last_checkpoint_result()));

    perf->stage_count = WATCH_APP_STAGE_COUNT;
    for (uint8_t i = 0U; i < WATCH_APP_STAGE_COUNT; ++i) {
        WebStatePerfStageSnapshot *stage = &perf->stages[i];
        copy_text(stage->name, sizeof(stage->name), watch_app_stage_name((WatchAppStageId)i));
        if (watch_app_get_stage_telemetry((WatchAppStageId)i, &telemetry)) {
            stage->budget_ms = telemetry.budget_ms;
            stage->last_duration_ms = telemetry.last_duration_ms;
            stage->max_duration_ms = telemetry.max_duration_ms;
            stage->sample_count = telemetry.sample_count;
            stage->over_budget_count = telemetry.over_budget_count;
            stage->consecutive_over_budget = telemetry.consecutive_over_budget;
            stage->max_consecutive_over_budget = telemetry.max_consecutive_over_budget;
            stage->deferred_count = telemetry.deferred_count;
        }
    }

    perf->history_count = watch_app_get_stage_history_count();
    if (perf->history_count > WEB_STATE_PERF_RECENT_HISTORY_MAX) {
        perf->history_count = WEB_STATE_PERF_RECENT_HISTORY_MAX;
    }
    for (uint8_t i = 0U; i < perf->history_count; ++i) {
        WebStatePerfHistorySnapshot *entry = &perf->history[i];
        if (!watch_app_get_stage_history_recent(i, &history)) {
            break;
        }
        copy_text(entry->stage, sizeof(entry->stage), watch_app_stage_name(history.stage));
        copy_text(entry->event,
                  sizeof(entry->event),
                  history.deferred != 0U ? "DEFER" : (history.over_budget != 0U ? "OVER" : "OK"));
        entry->timestamp_ms = history.timestamp_ms;
        entry->loop_counter = history.loop_counter;
        entry->duration_ms = history.duration_ms;
        entry->budget_ms = history.budget_ms;
    }
}

extern "C" bool web_runtime_snapshot_collect(WebRuntimeSnapshot *out)
{
    if (out == nullptr) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->uptime_ms = web_state_bridge_uptime_ms();
    out->has_device_config = storage_facade_get_device_config(&out->device_config);
    out->has_network_sync = network_sync_service_get_snapshot(&out->network_sync);
    out->has_time_source = time_service_get_source_snapshot(&out->time_source);
    out->has_ui_runtime = ui_get_runtime_snapshot(&out->ui_runtime);
    out->has_app_init_report = watch_app_get_init_report(&out->app_init_report);
    out->has_startup_report = system_get_startup_report(&out->startup_report);
    out->has_startup_status = system_get_startup_status(&out->startup_status);
    out->has_domain_state = model_get_domain_state(&out->domain_state) != NULL;

    {
        const SensorSnapshot *sensor_snapshot = sensor_service_get_snapshot();
        if (sensor_snapshot != nullptr) {
            out->sensor_snapshot = *sensor_snapshot;
            out->has_sensor_snapshot = true;
        }
    }

    out->web_control_plane_ready = web_server_control_plane_ready();
    out->web_console_ready = web_server_console_ready();
    out->filesystem_ready = web_server_filesystem_ready();
    out->filesystem_assets_ready = web_server_filesystem_assets_ready();
    out->asset_contract_ready = web_server_asset_contract_ready();
    copy_text(out->filesystem_status, sizeof(out->filesystem_status), web_server_filesystem_status());

    collect_sensor_detail(out);
    collect_diag_detail(out);
    collect_storage_detail(out);
    collect_alarm_music_overlay_detail(out);
    collect_perf_detail(out);
    out->has_detail_state = true;
    return true;
}
