#include "web/web_state_bridge_internal.h"
#include "web/web_wifi.h"
#include "services/network_sync_service.h"
#include <string.h>
#include <cstdio>
#include "web/web_runtime_snapshot.h"

extern "C" {
#include "ui_internal.h"
#include "services/time_service.h"
#include "watch_app.h"
#include "main.h"
#include "system_init.h"
}

static const char *web_page_name(PageId page)
{
    switch (page) {
        case PAGE_WATCHFACE: return "WATCH";
        case PAGE_QUICK: return "HUB";
        case PAGE_APPS: return "APPS";
        case PAGE_SETTINGS: return "SETTINGS";
        case PAGE_MUSIC: return "MELODY";
        case PAGE_SENSOR: return "SENSOR";
        case PAGE_DIAG: return "DIAG";
        case PAGE_STORAGE: return "STORAGE";
        case PAGE_GAMES: return "GAMES";
        case PAGE_GAME_DETAIL: return "GAME";
        default: return "SYSTEM";
    }
}

static void copy_system_stage_name(char *dst, size_t dst_size, SystemInitStage stage)
{
    if (dst == NULL || dst_size == 0U) {
        return;
    }

    strncpy(dst, system_init_stage_name(stage), dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static void collect_wifi_snapshot(WebStateCoreSnapshot *out, const WebRuntimeSnapshot *snapshot)
{
    const DeviceConfigSnapshot *device_cfg = &snapshot->device_config;
    out->wifi.connected = web_wifi_connected();
    out->wifi.provisioning_ap_active = web_wifi_access_point_active();
    out->wifi.config_wifi_ready = snapshot->has_device_config && device_cfg->wifi_configured;
    out->wifi.config_weather_ready = snapshot->has_device_config && device_cfg->weather_configured;
    out->wifi.auth_required = snapshot->has_device_config && device_cfg->api_token_configured;
    strncpy(out->wifi.mode, web_wifi_mode_name(), sizeof(out->wifi.mode) - 1U);
    strncpy(out->wifi.status, web_wifi_status_name(), sizeof(out->wifi.status) - 1U);
    strncpy(out->wifi.provisioning_ap_ssid, web_wifi_access_point_ssid(), sizeof(out->wifi.provisioning_ap_ssid) - 1U);
    strncpy(out->wifi.ip, web_wifi_ip_string(), sizeof(out->wifi.ip) - 1U);
    out->wifi.rssi = web_wifi_rssi();
}

static void collect_system_snapshot(WebStateCoreSnapshot *out, const WebRuntimeSnapshot *snapshot)
{
    out->system.uptime_ms = snapshot->uptime_ms;
    if (snapshot->has_app_init_report) {
        const WatchAppInitReport &init_report = snapshot->app_init_report;
        out->system.app_ready = init_report.success && init_report.failed_stage == WATCH_APP_INIT_STAGE_NONE;
        out->system.app_degraded = init_report.degraded;
        strncpy(out->system.app_init_stage,
                watch_app_init_stage_name(init_report.failed_stage != WATCH_APP_INIT_STAGE_NONE ?
                                          init_report.failed_stage : init_report.last_completed_stage),
                sizeof(out->system.app_init_stage) - 1U);
    } else {
        strncpy(out->system.app_init_stage, "UNKNOWN", sizeof(out->system.app_init_stage) - 1U);
    }

    if (snapshot->has_ui_runtime) {
        out->system.sleeping = snapshot->ui_runtime.sleeping;
        out->system.animating = snapshot->ui_runtime.animating;
        strncpy(out->system.current_page, web_page_name(snapshot->ui_runtime.current), sizeof(out->system.current_page) - 1U);
    } else {
        strncpy(out->system.current_page, "WATCH", sizeof(out->system.current_page) - 1U);
    }

    if (snapshot->has_time_source) {
        strncpy(out->system.time_source,
                time_service_source_name(snapshot->time_source.source),
                sizeof(out->system.time_source) - 1U);
        strncpy(out->system.time_confidence,
                time_service_confidence_name(snapshot->time_source.confidence),
                sizeof(out->system.time_confidence) - 1U);
        out->system.time_valid = snapshot->time_source.valid;
        out->system.time_authoritative = snapshot->time_source.authoritative;
        out->system.time_source_age_ms = snapshot->time_source.source_age_ms;
    } else {
        strncpy(out->system.time_source, "UNKNOWN", sizeof(out->system.time_source) - 1U);
        strncpy(out->system.time_confidence, "NONE", sizeof(out->system.time_confidence) - 1U);
    }

    if (snapshot->has_startup_report) {
        out->system.startup_ok = snapshot->startup_report.startup_ok;
        out->system.startup_degraded = snapshot->startup_report.degraded;
        out->system.fatal_stop_requested = snapshot->startup_report.fatal_stop_requested;
        copy_system_stage_name(out->system.startup_failure_stage,
                               sizeof(out->system.startup_failure_stage),
                               snapshot->startup_report.failure_stage);
        copy_system_stage_name(out->system.startup_recovery_stage,
                               sizeof(out->system.startup_recovery_stage),
                               snapshot->startup_report.recovery_stage);
    } else if (snapshot->has_startup_status) {
        out->system.startup_ok = !snapshot->startup_status.init_failed;
        out->system.startup_degraded = snapshot->startup_status.safe_mode_boot_recovery_pending;
        out->system.fatal_stop_requested = snapshot->startup_status.init_failed;
        if (snapshot->startup_status.init_failed) {
            strncpy(out->system.startup_failure_stage, "UNKNOWN", sizeof(out->system.startup_failure_stage) - 1U);
            out->system.startup_failure_stage[sizeof(out->system.startup_failure_stage) - 1U] = '\0';
        } else {
            copy_system_stage_name(out->system.startup_failure_stage,
                                   sizeof(out->system.startup_failure_stage),
                                   SYSTEM_INIT_STAGE_NONE);
        }
        copy_system_stage_name(out->system.startup_recovery_stage,
                               sizeof(out->system.startup_recovery_stage),
                               snapshot->startup_status.safe_mode_boot_recovery_stage);
    } else {
        strncpy(out->system.startup_failure_stage, "NONE", sizeof(out->system.startup_failure_stage) - 1U);
        strncpy(out->system.startup_recovery_stage, "NONE", sizeof(out->system.startup_recovery_stage) - 1U);
    }
}

static void collect_model_summary(WebStateCoreSnapshot *out, const WebRuntimeSnapshot *snapshot)
{
    if (snapshot->has_domain_state) {
        const ModelDomainState &domain_state = snapshot->domain_state;
        out->activity.steps = domain_state.activity.steps;
        out->activity.goal = domain_state.activity.goal;
        out->activity.goal_percent = (domain_state.activity.goal == 0U) ? 0U :
            (uint8_t)(((uint64_t)domain_state.activity.steps * 100ULL) / domain_state.activity.goal);
        snprintf(out->system.system_face, sizeof(out->system.system_face), "F%u", domain_state.settings.watchface + 1U);
        snprintf(out->system.brightness_label, sizeof(out->system.brightness_label), "%u/4", domain_state.settings.brightness + 1U);
        snprintf(out->activity.activity_label, sizeof(out->activity.activity_label), "%u%%", out->activity.goal_percent);
    } else {
        strncpy(out->system.system_face, "F-", sizeof(out->system.system_face) - 1U);
        strncpy(out->system.brightness_label, "-", sizeof(out->system.brightness_label) - 1U);
        strncpy(out->activity.activity_label, "0%", sizeof(out->activity.activity_label) - 1U);
    }
}

static void collect_status_labels(WebStateCoreSnapshot *out)
{
    ui_status_compose_header_tags(out->system.header_tags, sizeof(out->system.header_tags));
    ui_status_compose_network_value(out->weather.network_line, sizeof(out->weather.network_line),
                                    out->weather.network_subline, sizeof(out->weather.network_subline));
    ui_status_compose_alarm_value(out->labels.alarm_label, sizeof(out->labels.alarm_label));
    ui_status_compose_music_value(out->labels.music_label, sizeof(out->labels.music_label));
    ui_status_compose_sensor_value(out->labels.sensor_label, sizeof(out->labels.sensor_label));
    ui_status_compose_storage_value(out->labels.storage_label, sizeof(out->labels.storage_label));
    ui_status_compose_diag_value(out->labels.diag_label, sizeof(out->labels.diag_label));
    if (out->labels.sensor_label[0] == '\0') {
        strncpy(out->labels.sensor_label, "UNKNOWN", sizeof(out->labels.sensor_label) - 1U);
    }
    if (out->labels.storage_label[0] == '\0') {
        strncpy(out->labels.storage_label, "UNKNOWN", sizeof(out->labels.storage_label) - 1U);
    }
    if (out->labels.diag_label[0] == '\0') {
        strncpy(out->labels.diag_label, "OK", sizeof(out->labels.diag_label) - 1U);
    }
}

static void collect_weather_snapshot(WebStateCoreSnapshot *out, const WebRuntimeSnapshot *snapshot)
{
    if (snapshot->has_network_sync) {
        const NetworkSyncSnapshot &net_snapshot = snapshot->network_sync;
        out->weather.valid = net_snapshot.weather_valid;
        out->weather.temperature_tenths_c = net_snapshot.temperature_tenths_c;
        strncpy(out->weather.text, net_snapshot.weather_text, sizeof(out->weather.text) - 1U);
        strncpy(out->weather.location, net_snapshot.location_name, sizeof(out->weather.location) - 1U);
        out->weather.updated_at_ms = net_snapshot.last_weather_sync_ms;
        out->weather.tls_verified = net_snapshot.weather_tls_verified;
        out->weather.tls_ca_loaded = net_snapshot.weather_ca_loaded;
        out->weather.last_http_status = net_snapshot.last_weather_http_status;
        strncpy(out->weather.tls_mode, net_snapshot.weather_tls_mode, sizeof(out->weather.tls_mode) - 1U);
        strncpy(out->weather.sync_status, net_snapshot.weather_status, sizeof(out->weather.sync_status) - 1U);
    }

    if (!out->system.app_ready) {
        strncpy(out->weather.network_label, "INIT", sizeof(out->weather.network_label) - 1U);
    } else if (out->wifi.connected && out->weather.valid) {
        strncpy(out->weather.network_label, "READY", sizeof(out->weather.network_label) - 1U);
    } else if (out->wifi.connected) {
        strncpy(out->weather.network_label, "SYNC", sizeof(out->weather.network_label) - 1U);
    } else if (out->wifi.provisioning_ap_active) {
        strncpy(out->weather.network_label, "PROVISION", sizeof(out->weather.network_label) - 1U);
    } else {
        strncpy(out->weather.network_label, "OFFLINE", sizeof(out->weather.network_label) - 1U);
    }
}

extern "C" bool web_state_core_collect_from_runtime_snapshot(const WebRuntimeSnapshot *snapshot, WebStateCoreSnapshot *out)
{
    if (snapshot == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    collect_wifi_snapshot(out, snapshot);
    collect_system_snapshot(out, snapshot);
    collect_model_summary(out, snapshot);
    collect_status_labels(out);
    collect_weather_snapshot(out, snapshot);
    return true;
}

extern "C" bool web_state_core_collect_impl(WebStateCoreSnapshot *out)
{
    WebRuntimeSnapshot snapshot = {};

    if (out == NULL) {
        return false;
    }

    if (!web_runtime_snapshot_collect(&snapshot)) {
        return false;
    }

    return web_state_core_collect_from_runtime_snapshot(&snapshot, out);
}
