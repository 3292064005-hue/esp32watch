#include "web/web_state_bridge_internal.h"
#include "web/web_wifi.h"
#include "services/network_sync_service.h"
#include <string.h>
#include <cstdio>

extern "C" {
#include "ui_internal.h"
#include "services/device_config.h"
#include "services/time_service.h"
#include "watch_app.h"
#include "main.h"
}

static const char *web_page_name(PageId page)
{
    switch (page) {
        case PAGE_WATCHFACE: return "WATCH";
        case PAGE_QUICK: return "HUB";
        case PAGE_APPS: return "APPS";
        case PAGE_SETTINGS: return "SETTINGS";
        case PAGE_MUSIC: return "MUSIC";
        case PAGE_SENSOR: return "SENSOR";
        case PAGE_DIAG: return "DIAG";
        case PAGE_STORAGE: return "STORAGE";
        case PAGE_GAMES: return "GAMES";
        case PAGE_GAME_DETAIL: return "GAME";
        default: return "SYSTEM";
    }
}

static void collect_wifi_snapshot(WebStateCoreSnapshot *out, const DeviceConfigSnapshot *device_cfg)
{
    out->wifi.connected = web_wifi_connected();
    out->wifi.provisioning_ap_active = web_wifi_access_point_active();
    out->wifi.config_wifi_ready = device_cfg->wifi_configured;
    out->wifi.config_weather_ready = device_cfg->weather_configured;
    out->wifi.auth_required = device_cfg->api_token_configured;
    strncpy(out->wifi.mode, web_wifi_mode_name(), sizeof(out->wifi.mode) - 1U);
    strncpy(out->wifi.status, web_wifi_status_name(), sizeof(out->wifi.status) - 1U);
    strncpy(out->wifi.provisioning_ap_ssid, web_wifi_access_point_ssid(), sizeof(out->wifi.provisioning_ap_ssid) - 1U);
    strncpy(out->wifi.ip, web_wifi_ip_string(), sizeof(out->wifi.ip) - 1U);
    out->wifi.rssi = web_wifi_rssi();
}

static void collect_system_snapshot(WebStateCoreSnapshot *out)
{
    UiRuntimeSnapshot ui_runtime;
    TimeSourceSnapshot time_snapshot;
    WatchAppInitReport init_report;

    out->system.uptime_ms = web_state_bridge_uptime_ms();
    memset(&init_report, 0, sizeof(init_report));
    if (watch_app_get_init_report(&init_report)) {
        out->system.app_ready = init_report.success && init_report.failed_stage == WATCH_APP_INIT_STAGE_NONE;
        out->system.app_degraded = init_report.degraded;
        strncpy(out->system.app_init_stage,
                watch_app_init_stage_name(init_report.failed_stage != WATCH_APP_INIT_STAGE_NONE ?
                                          init_report.failed_stage : init_report.last_completed_stage),
                sizeof(out->system.app_init_stage) - 1U);
    } else {
        strncpy(out->system.app_init_stage, "UNKNOWN", sizeof(out->system.app_init_stage) - 1U);
    }

    if (ui_get_runtime_snapshot(&ui_runtime)) {
        out->system.sleeping = ui_runtime.sleeping;
        out->system.animating = ui_runtime.animating;
        strncpy(out->system.current_page, web_page_name(ui_runtime.current), sizeof(out->system.current_page) - 1U);
    } else {
        strncpy(out->system.current_page, "WATCH", sizeof(out->system.current_page) - 1U);
    }

    if (time_service_get_source_snapshot(&time_snapshot)) {
        strncpy(out->system.time_source,
                time_service_source_name(time_snapshot.source),
                sizeof(out->system.time_source) - 1U);
        strncpy(out->system.time_confidence,
                time_service_confidence_name(time_snapshot.confidence),
                sizeof(out->system.time_confidence) - 1U);
        out->system.time_valid = time_snapshot.valid;
        out->system.time_authoritative = time_snapshot.authoritative;
        out->system.time_source_age_ms = time_snapshot.source_age_ms;
    } else {
        strncpy(out->system.time_source, "UNKNOWN", sizeof(out->system.time_source) - 1U);
        strncpy(out->system.time_confidence, "NONE", sizeof(out->system.time_confidence) - 1U);
    }
}

static void collect_model_summary(WebStateCoreSnapshot *out)
{
    ModelDomainState domain_state;

    if (model_get_domain_state(&domain_state) != NULL) {
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

static void collect_weather_snapshot(WebStateCoreSnapshot *out)
{
    NetworkSyncSnapshot net_snapshot;

    if (network_sync_service_get_snapshot(&net_snapshot)) {
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

extern "C" bool web_state_core_collect_impl(WebStateCoreSnapshot *out)
{
    DeviceConfigSnapshot device_cfg;

    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    device_config_init();
    (void)device_config_get(&device_cfg);
    collect_wifi_snapshot(out, &device_cfg);
    collect_system_snapshot(out);
    collect_model_summary(out);
    collect_status_labels(out);
    collect_weather_snapshot(out);
    return true;
}
