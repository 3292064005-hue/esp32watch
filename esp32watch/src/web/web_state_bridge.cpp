#include "web/web_state_bridge.h"
#include "web/web_wifi.h"
#include "web/web_overlay.h"
#include "services/network_sync_service.h"
#include <string.h>
#include <Arduino.h>

extern "C" {
#include "display.h"
#include "services/time_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"
#include "watch_app.h"
#include "main.h"
#include "app_command.h"
}

static uint32_t g_startup_ms = 0;

void web_state_bridge_mark_startup(uint32_t mark_ms)
{
    if (g_startup_ms == 0) {
        g_startup_ms = mark_ms;
    }
}

bool web_state_snapshot_collect(WebStateSnapshot *out)
{
    if (!out) {
        return false;
    }

    memset(out, 0, sizeof(WebStateSnapshot));

    uint32_t now_ms = platform_time_now_ms();
    TimeSourceSnapshot time_snapshot;
    NetworkSyncSnapshot net_snapshot;
    ModelDomainState domain_state;
    const SensorSnapshot *sensor_snapshot = sensor_service_get_snapshot();
    DiagFaultCode last_fault_code = DIAG_FAULT_NONE;
    DiagFaultInfo last_fault_info;
    DiagLogEntry last_log;
    if (g_startup_ms == 0) {
        g_startup_ms = now_ms;
    }

    out->uptime_ms = now_ms - g_startup_ms;

    out->wifi_connected = web_wifi_connected();
    strncpy(out->ip, web_wifi_ip_string(), sizeof(out->ip) - 1);
    out->rssi = web_wifi_rssi();

    out->app_ready = true;

    out->sleeping = false;
    out->animating = false;
    strncpy(out->current_page, "MENU", sizeof(out->current_page) - 1);
    if (time_service_get_source_snapshot(&time_snapshot)) {
        strncpy(out->time_source, time_service_source_name(time_snapshot.source), sizeof(out->time_source) - 1);
    } else {
        strncpy(out->time_source, "UNKNOWN", sizeof(out->time_source) - 1);
    }

    if (model_get_domain_state(&domain_state) != NULL) {
        out->steps = domain_state.activity.steps;
        out->goal = domain_state.activity.goal;
        out->goal_percent = (domain_state.activity.goal == 0U) ? 0U :
            (uint8_t)(((uint64_t)domain_state.activity.steps * 100ULL) / domain_state.activity.goal);
    } else {
        out->steps = 0;
        out->goal = 0;
        out->goal_percent = 0;
    }

    if (sensor_snapshot != NULL) {
        out->sensor_online = sensor_snapshot->online;
        out->sensor_calibrated = sensor_snapshot->calibrated;
        out->sensor_static_now = sensor_snapshot->static_now;
        out->sensor_quality = sensor_snapshot->quality;
        out->sensor_error_code = sensor_snapshot->error_code;
        out->sensor_fault_count = sensor_snapshot->fault_count;
        out->sensor_reinit_count = sensor_snapshot->reinit_count;
        out->sensor_calibration_progress = sensor_snapshot->calibration_progress;
        strncpy(out->sensor_runtime_state,
                sensor_runtime_state_name(sensor_snapshot->runtime_state),
                sizeof(out->sensor_runtime_state) - 1);
        strncpy(out->sensor_calibration_state,
                sensor_calibration_state_name(sensor_snapshot->calibration_state),
                sizeof(out->sensor_calibration_state) - 1);
        out->sensor_ax = sensor_snapshot->ax;
        out->sensor_ay = sensor_snapshot->ay;
        out->sensor_az = sensor_snapshot->az;
        out->sensor_gx = sensor_snapshot->gx;
        out->sensor_gy = sensor_snapshot->gy;
        out->sensor_gz = sensor_snapshot->gz;
        out->sensor_accel_norm_mg = sensor_snapshot->accel_norm_mg;
        out->sensor_baseline_mg = sensor_snapshot->baseline_mg;
        out->sensor_motion_score = sensor_snapshot->motion_score;
        out->sensor_last_sample_ms = sensor_snapshot->last_sample_ms;
        out->sensor_steps_total = sensor_snapshot->steps_total;
        out->pitch_deg = (float)sensor_snapshot->pitch_deg;
        out->roll_deg = (float)sensor_snapshot->roll_deg;
    } else {
        out->sensor_online = false;
        out->sensor_calibrated = false;
        strncpy(out->sensor_runtime_state, "UNAVAILABLE", sizeof(out->sensor_runtime_state) - 1);
        strncpy(out->sensor_calibration_state, "N/A", sizeof(out->sensor_calibration_state) - 1);
        out->pitch_deg = 0.0f;
        out->roll_deg = 0.0f;
    }

    out->safe_mode_active = false;
    out->safe_mode_can_clear = true;
    strncpy(out->safe_mode_reason, "NONE", sizeof(out->safe_mode_reason) - 1);
    if (diag_service_is_safe_mode_active()) {
        out->safe_mode_active = true;
        out->safe_mode_can_clear = diag_service_can_clear_safe_mode();
        strncpy(out->safe_mode_reason,
                diag_service_safe_mode_reason_name(diag_service_get_safe_mode_reason()),
                sizeof(out->safe_mode_reason) - 1);
    }

    out->has_last_fault = false;
    strncpy(out->last_fault_name, "NONE", sizeof(out->last_fault_name) - 1);
    strncpy(out->last_fault_severity, "INFO", sizeof(out->last_fault_severity) - 1);
    out->last_fault_count = 0;
    if (diag_service_get_last_fault(&last_fault_code, &last_fault_info)) {
        out->has_last_fault = true;
        strncpy(out->last_fault_name, diag_service_fault_name(last_fault_code), sizeof(out->last_fault_name) - 1);
        strncpy(out->last_fault_severity,
                diag_service_fault_severity_name(last_fault_info.severity),
                sizeof(out->last_fault_severity) - 1);
        out->last_fault_count = last_fault_info.count;
    }

    strncpy(out->storage_backend, storage_service_get_backend_name(), sizeof(out->storage_backend) - 1);
    strncpy(out->storage_commit_state,
            storage_service_commit_state_name(storage_service_get_commit_state()),
            sizeof(out->storage_commit_state) - 1);
    out->storage_transaction_active = storage_service_is_transaction_active();
    out->storage_sleep_flush_pending = storage_service_is_sleep_flush_pending();

    out->display_present_count = display_get_present_count();
    out->display_tx_fail_count = display_get_tx_error_count();
    if (network_sync_service_get_snapshot(&net_snapshot)) {
        out->weather_valid = net_snapshot.weather_valid;
        out->weather_temperature_tenths_c = net_snapshot.temperature_tenths_c;
        strncpy(out->weather_text, net_snapshot.weather_text, sizeof(out->weather_text) - 1);
        strncpy(out->weather_location, net_snapshot.location_name, sizeof(out->weather_location) - 1);
        out->weather_updated_at_ms = net_snapshot.last_weather_sync_ms;
    }

    out->has_last_log = diag_service_get_last_log(&last_log);
    strncpy(out->last_log_name,
            out->has_last_log ? diag_service_event_name((DiagEventCode)last_log.code) : "NONE",
            sizeof(out->last_log_name) - 1);
    out->last_log_value = out->has_last_log ? last_log.value : 0;
    out->last_log_aux = out->has_last_log ? last_log.aux : 0;

    strncpy(out->last_overlay_text, web_overlay_last_text(), sizeof(out->last_overlay_text) - 1);
    out->overlay_active = web_overlay_is_active(now_ms);
    out->overlay_expire_at_ms = web_overlay_expire_at_ms();

    return true;
}

