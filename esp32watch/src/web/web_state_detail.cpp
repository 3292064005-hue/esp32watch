#include "web/web_state_bridge_internal.h"
#include "web/web_overlay.h"
#include "melody_service.h"
#include <string.h>
#include <cstdio>

extern "C" {
#include "display.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include "watch_app.h"
#include "main.h"
}

static void collect_sensor_snapshot(WebStateDetailSnapshot *out)
{
    const SensorSnapshot *sensor_snapshot = sensor_service_get_snapshot();

    if (sensor_snapshot != NULL) {
        out->sensor.online = sensor_snapshot->online;
        out->sensor.calibrated = sensor_snapshot->calibrated;
        out->sensor.static_now = sensor_snapshot->static_now;
        out->sensor.quality = sensor_snapshot->quality;
        out->sensor.error_code = sensor_snapshot->error_code;
        out->sensor.fault_count = sensor_snapshot->fault_count;
        out->sensor.reinit_count = sensor_snapshot->reinit_count;
        out->sensor.calibration_progress = sensor_snapshot->calibration_progress;
        strncpy(out->sensor.runtime_state,
                sensor_runtime_state_name(sensor_snapshot->runtime_state),
                sizeof(out->sensor.runtime_state) - 1U);
        strncpy(out->sensor.calibration_state,
                sensor_calibration_state_name(sensor_snapshot->calibration_state),
                sizeof(out->sensor.calibration_state) - 1U);
        out->sensor.ax = sensor_snapshot->ax;
        out->sensor.ay = sensor_snapshot->ay;
        out->sensor.az = sensor_snapshot->az;
        out->sensor.gx = sensor_snapshot->gx;
        out->sensor.gy = sensor_snapshot->gy;
        out->sensor.gz = sensor_snapshot->gz;
        out->sensor.accel_norm_mg = sensor_snapshot->accel_norm_mg;
        out->sensor.baseline_mg = sensor_snapshot->baseline_mg;
        out->sensor.motion_score = sensor_snapshot->motion_score;
        out->sensor.last_sample_ms = sensor_snapshot->last_sample_ms;
        out->sensor.steps_total = sensor_snapshot->steps_total;
        out->sensor.pitch_deg = (float)sensor_snapshot->pitch_deg;
        out->sensor.roll_deg = (float)sensor_snapshot->roll_deg;
    } else {
        strncpy(out->sensor.runtime_state, "UNAVAILABLE", sizeof(out->sensor.runtime_state) - 1U);
        strncpy(out->sensor.calibration_state, "N/A", sizeof(out->sensor.calibration_state) - 1U);
    }
}

static void collect_diag_snapshot(WebStateDetailSnapshot *out)
{
    DiagFaultCode last_fault_code = DIAG_FAULT_NONE;
    DiagFaultInfo last_fault_info;
    DiagLogEntry last_log;

    out->diag.safe_mode_can_clear = true;
    strncpy(out->diag.safe_mode_reason, "NONE", sizeof(out->diag.safe_mode_reason) - 1U);
    if (diag_service_is_safe_mode_active()) {
        out->diag.safe_mode_active = true;
        out->diag.safe_mode_can_clear = diag_service_can_clear_safe_mode();
        strncpy(out->diag.safe_mode_reason,
                diag_service_safe_mode_reason_name(diag_service_get_safe_mode_reason()),
                sizeof(out->diag.safe_mode_reason) - 1U);
    }

    strncpy(out->diag.last_fault_name, "NONE", sizeof(out->diag.last_fault_name) - 1U);
    strncpy(out->diag.last_fault_severity, "INFO", sizeof(out->diag.last_fault_severity) - 1U);
    if (diag_service_get_last_fault(&last_fault_code, &last_fault_info)) {
        out->diag.has_last_fault = true;
        strncpy(out->diag.last_fault_name, diag_service_fault_name(last_fault_code), sizeof(out->diag.last_fault_name) - 1U);
        strncpy(out->diag.last_fault_severity,
                diag_service_fault_severity_name(last_fault_info.severity),
                sizeof(out->diag.last_fault_severity) - 1U);
        out->diag.last_fault_count = last_fault_info.count;
    }

    out->diag.has_last_log = diag_service_get_last_log(&last_log);
    strncpy(out->diag.last_log_name,
            out->diag.has_last_log ? diag_service_event_name((DiagEventCode)last_log.code) : "NONE",
            sizeof(out->diag.last_log_name) - 1U);
    out->diag.last_log_value = out->diag.has_last_log ? last_log.value : 0U;
    out->diag.last_log_aux = out->diag.has_last_log ? last_log.aux : 0U;
}

static void collect_storage_snapshot(WebStateDetailSnapshot *out)
{
    strncpy(out->storage.backend, storage_service_get_backend_name(), sizeof(out->storage.backend) - 1U);
    strncpy(out->storage.backend_phase, storage_service_backend_phase_name(), sizeof(out->storage.backend_phase) - 1U);
    strncpy(out->storage.commit_state,
            storage_service_commit_state_name(storage_service_get_commit_state()),
            sizeof(out->storage.commit_state) - 1U);
    out->storage.schema_version = storage_service_get_schema_version();
    out->storage.flash_supported = storage_service_flash_supported();
    out->storage.flash_ready = storage_service_is_flash_backend_ready();
    out->storage.migration_attempted = storage_service_was_migration_attempted();
    out->storage.migration_ok = storage_service_last_migration_ok();
    out->storage.transaction_active = storage_service_is_transaction_active();
    out->storage.sleep_flush_pending = storage_service_is_sleep_flush_pending();
}

static void collect_display_alarm_music_overlay(WebStateDetailSnapshot *out)
{
    uint32_t now_ms = platform_time_now_ms();
    out->display.present_count = display_get_present_count();
    out->display.tx_fail_count = display_get_tx_error_count();

    out->alarm.next_alarm_index = model_find_next_enabled_alarm();
    if (out->alarm.next_alarm_index < APP_MAX_ALARMS) {
        const AlarmState *alarm = model_get_alarm(out->alarm.next_alarm_index);
        out->alarm.enabled = alarm->enabled;
        out->alarm.ringing = alarm->ringing;
        snprintf(out->alarm.next_time, sizeof(out->alarm.next_time), "%02u:%02u", alarm->hour, alarm->minute);
    } else {
        out->alarm.next_alarm_index = 0xFFU;
        strncpy(out->alarm.next_time, "--:--", sizeof(out->alarm.next_time) - 1U);
    }

    out->music.available = melody_is_available();
    out->music.playing = melody_is_playing();
    strncpy(out->music.state, melody_state_name(melody_get_state()), sizeof(out->music.state) - 1U);
    strncpy(out->music.song, melody_song_ascii_name(melody_get_current_song()), sizeof(out->music.song) - 1U);

    strncpy(out->overlay.text, web_overlay_last_text(), sizeof(out->overlay.text) - 1U);
    out->overlay.active = web_overlay_is_active(now_ms);
    out->overlay.expire_at_ms = web_overlay_expire_at_ms();
}

extern "C" bool web_state_detail_collect_impl(WebStateDetailSnapshot *out)
{
    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    collect_sensor_snapshot(out);
    collect_diag_snapshot(out);
    collect_storage_snapshot(out);
    collect_display_alarm_music_overlay(out);
    return true;
}
