#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include <cstring>

#include "web/web_json.h"
#include "web/web_contract.h"
#include "web/web_routes_api_internal.h"
#include "web/web_runtime_snapshot.h"
#include "web/web_state_bridge.h"
#include "web/web_state_bridge_internal.h"

extern "C" {
#include "services/device_config.h"
}

static uint32_t web_hash_bytes_fnv1a32(const uint8_t *data, size_t size)
{
    const uint32_t kOffset = 2166136261UL;
    const uint32_t kPrime = 16777619UL;
    uint32_t hash = kOffset;

    if (data == nullptr || size == 0U) {
        return 0U;
    }

    for (size_t i = 0U; i < size; ++i) {
        hash ^= data[i];
        hash *= kPrime;
    }
    return hash;
}

static void append_perf_payload(String &response, const WebStatePerfSnapshot &perf, bool include_history)
{
    response += "\"perf\":{";
    web_json_kv_u32(response, "loopCount", perf.loop_count, false);
    web_json_kv_u32(response, "maxLoopMs", perf.max_loop_ms, false);
    web_json_kv_u32(response, "actionQueueDepth", perf.action_queue_depth, false);
    web_json_kv_u32(response, "actionQueueDropCount", perf.action_queue_drop_count, false);
    web_json_kv_str(response, "lastCheckpoint", perf.last_checkpoint, false);
    web_json_kv_str(response, "lastCheckpointResult", perf.last_checkpoint_result, false);
    web_json_kv_u32(response, "stageCount", perf.stage_count, false);
    web_json_kv_u32(response, "historyCount", perf.history_count, false);
    response += "\"stages\":";
    response += "[";
    for (uint8_t i = 0U; i < perf.stage_count; ++i) {
        const WebStatePerfStageSnapshot &stage = perf.stages[i];
        if (i != 0U) {
            response += ",";
        }
        response += "{";
        web_json_kv_str(response, "name", stage.name, false);
        web_json_kv_u32(response, "budgetMs", stage.budget_ms, false);
        web_json_kv_u32(response, "lastDurationMs", stage.last_duration_ms, false);
        web_json_kv_u32(response, "maxDurationMs", stage.max_duration_ms, false);
        web_json_kv_u32(response, "sampleCount", stage.sample_count, false);
        web_json_kv_u32(response, "overBudgetCount", stage.over_budget_count, false);
        web_json_kv_u32(response, "consecutiveOverBudget", stage.consecutive_over_budget, false);
        web_json_kv_u32(response, "maxConsecutiveOverBudget", stage.max_consecutive_over_budget, false);
        web_json_kv_u32(response, "deferredCount", stage.deferred_count, true);
        response += "}";
    }
    response += "]";
    if (include_history) {
        response += ",\"history\":";
        response += "[";
        for (uint8_t i = 0U; i < perf.history_count; ++i) {
            const WebStatePerfHistorySnapshot &entry = perf.history[i];
            if (i != 0U) {
                response += ",";
            }
            response += "{";
            web_json_kv_str(response, "stage", entry.stage, false);
            web_json_kv_str(response, "event", entry.event, false);
            web_json_kv_u32(response, "timestampMs", entry.timestamp_ms, false);
            web_json_kv_u32(response, "loopCounter", entry.loop_counter, false);
            web_json_kv_u32(response, "durationMs", entry.duration_ms, false);
            web_json_kv_u32(response, "budgetMs", entry.budget_ms, true);
            response += "}";
        }
        response += "]";
    }
    response += "}";
}

typedef struct {
    WebRuntimeSnapshot runtime;
    WebStateCoreSnapshot core;
    WebStateDetailSnapshot detail;
    const DeviceConfigSnapshot *cfg;
} WebApiStateBundle;

typedef enum {
    WEB_STATE_SECTION_WIFI = 0,
    WEB_STATE_SECTION_SYSTEM,
    WEB_STATE_SECTION_CONFIG,
    WEB_STATE_SECTION_ACTIVITY,
    WEB_STATE_SECTION_ALARM,
    WEB_STATE_SECTION_MUSIC,
    WEB_STATE_SECTION_SENSOR,
    WEB_STATE_SECTION_STORAGE,
    WEB_STATE_SECTION_DIAG,
    WEB_STATE_SECTION_DISPLAY,
    WEB_STATE_SECTION_WEATHER,
    WEB_STATE_SECTION_SUMMARY,
    WEB_STATE_SECTION_TERMINAL,
    WEB_STATE_SECTION_OVERLAY,
    WEB_STATE_SECTION_PERF,
    WEB_STATE_SECTION_STARTUP_RAW,
    WEB_STATE_SECTION_QUEUE_RAW
} WebStateSectionId;

typedef enum {
    WEB_STATE_SECTION_FLAG_NONE = 0,
    WEB_STATE_SECTION_FLAG_WIFI_INCLUDE_RSSI = 1 << 0,
    WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_APP_INIT_STAGE = 1 << 1,
    WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SLEEP_STATE = 1 << 2,
    WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SAFE_MODE = 1 << 3,
    WEB_STATE_SECTION_FLAG_SENSOR_INCLUDE_RAW_AXES = 1 << 4,
    WEB_STATE_SECTION_FLAG_DIAG_INCLUDE_SAFE_MODE_FIELDS = 1 << 5,
    WEB_STATE_SECTION_FLAG_WEATHER_INCLUDE_UPDATED_AT = 1 << 6,
    WEB_STATE_SECTION_FLAG_SUMMARY_INCLUDE_NETWORK_LABEL = 1 << 7,
    WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY = 1 << 8,
} WebStateSectionFlags;

typedef struct {
    WebStateSectionId id;
    uint16_t flags;
} WebStateSectionSpec;

static bool collect_api_state_bundle(WebApiStateBundle *out, bool require_device_config)
{
    if (out == nullptr) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    if (!web_runtime_snapshot_collect(&out->runtime) ||
        !web_state_core_collect_from_runtime_snapshot(&out->runtime, &out->core) ||
        !web_state_detail_collect_from_runtime_snapshot(&out->runtime, &out->detail)) {
        return false;
    }

    if (require_device_config) {
        if (!out->runtime.has_device_config) {
            return false;
        }
        out->cfg = &out->runtime.device_config;
    }
    return true;
}

static void append_state_versions(String &response)
{
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    web_json_kv_u32(response, "stateVersion", WEB_STATE_VERSION, false);
}

enum WebStatePayloadView {
    WEB_STATE_PAYLOAD_DETAIL = 0,
    WEB_STATE_PAYLOAD_PERF,
    WEB_STATE_PAYLOAD_RAW,
    WEB_STATE_PAYLOAD_AGGREGATE,
};

static const WebStateSectionSpec g_web_state_view_detail[] = {
    {WEB_STATE_SECTION_ACTIVITY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_ALARM, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_MUSIC, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_SENSOR, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_STORAGE, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_DIAG, WEB_STATE_SECTION_FLAG_DIAG_INCLUDE_SAFE_MODE_FIELDS},
    {WEB_STATE_SECTION_DISPLAY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_WEATHER, WEB_STATE_SECTION_FLAG_WEATHER_INCLUDE_UPDATED_AT},
    {WEB_STATE_SECTION_TERMINAL, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_OVERLAY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_PERF, WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY},
};

static const WebStateSectionSpec g_web_state_view_perf[] = {
    {WEB_STATE_SECTION_PERF, WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY},
};

static const WebStateSectionSpec g_web_state_view_raw[] = {
    {WEB_STATE_SECTION_STARTUP_RAW, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_QUEUE_RAW, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_SENSOR, WEB_STATE_SECTION_FLAG_SENSOR_INCLUDE_RAW_AXES},
    {WEB_STATE_SECTION_STORAGE, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_DISPLAY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_PERF, WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY},
};

static const WebStateSectionSpec g_web_state_view_aggregate[] = {
    {WEB_STATE_SECTION_WIFI, WEB_STATE_SECTION_FLAG_WIFI_INCLUDE_RSSI},
    {WEB_STATE_SECTION_SYSTEM, WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_APP_INIT_STAGE | WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SLEEP_STATE | WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SAFE_MODE},
    {WEB_STATE_SECTION_CONFIG, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_ACTIVITY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_ALARM, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_MUSIC, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_SENSOR, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_STORAGE, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_DIAG, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_DISPLAY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_WEATHER, WEB_STATE_SECTION_FLAG_WEATHER_INCLUDE_UPDATED_AT},
    {WEB_STATE_SECTION_SUMMARY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_TERMINAL, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_OVERLAY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_PERF, WEB_STATE_SECTION_FLAG_NONE},
};

static const WebStateSectionSpec *web_state_view_schema(WebStatePayloadView view, size_t *out_count)
{
    const WebStateSectionSpec *schema = g_web_state_view_aggregate;
    size_t count = sizeof(g_web_state_view_aggregate) / sizeof(g_web_state_view_aggregate[0]);

    switch (view) {
        case WEB_STATE_PAYLOAD_DETAIL:
            schema = g_web_state_view_detail;
            count = sizeof(g_web_state_view_detail) / sizeof(g_web_state_view_detail[0]);
            break;
        case WEB_STATE_PAYLOAD_PERF:
            schema = g_web_state_view_perf;
            count = sizeof(g_web_state_view_perf) / sizeof(g_web_state_view_perf[0]);
            break;
        case WEB_STATE_PAYLOAD_RAW:
            schema = g_web_state_view_raw;
            count = sizeof(g_web_state_view_raw) / sizeof(g_web_state_view_raw[0]);
            break;
        case WEB_STATE_PAYLOAD_AGGREGATE:
        default:
            break;
    }

    if (out_count != nullptr) {
        *out_count = count;
    }
    return schema;
}

static void append_state_wifi_section(String &response,
                                      const WebStateWifiSnapshot &wifi,
                                      bool include_rssi,
                                      bool last)
{
    response += "\"wifi\":{";
    web_json_kv_bool(response, "connected", wifi.connected, false);
    web_json_kv_bool(response, "provisioningApActive", wifi.provisioning_ap_active, false);
    web_json_kv_str(response, "provisioningApSsid", wifi.provisioning_ap_ssid, false);
    web_json_kv_str(response, "mode", wifi.mode, false);
    web_json_kv_str(response, "status", wifi.status, false);
    web_json_kv_str(response, "ip", wifi.ip, include_rssi);
    if (include_rssi) {
        web_json_kv_i32(response, "rssi", wifi.rssi, true);
    }
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_system_section(String &response,
                                        const WebStateCoreSnapshot &core,
                                        bool include_app_init_stage,
                                        bool include_sleep_state,
                                        bool include_safe_mode,
                                        bool safe_mode_value,
                                        bool last)
{
    response += "\"system\":{";
    web_json_kv_u32(response, "uptimeMs", core.system.uptime_ms, false);
    web_json_kv_bool(response, "appReady", core.system.app_ready, false);
    web_json_kv_bool(response, "appDegraded", core.system.app_degraded, false);
    web_json_kv_bool(response, "startupOk", core.system.startup_ok, false);
    web_json_kv_bool(response, "startupDegraded", core.system.startup_degraded, false);
    web_json_kv_bool(response, "fatalStopRequested", core.system.fatal_stop_requested, false);
    web_json_kv_str(response, "startupFailureStage", core.system.startup_failure_stage, false);
    web_json_kv_str(response, "startupRecoveryStage", core.system.startup_recovery_stage, false);
    if (include_app_init_stage) {
        web_json_kv_str(response, "appInitStage", core.system.app_init_stage, false);
    }
    web_json_kv_str(response, "page", core.system.current_page, false);
    web_json_kv_str(response, "timeSource", core.system.time_source, false);
    web_json_kv_str(response, "timeConfidence", core.system.time_confidence, false);
    web_json_kv_str(response, "timeAuthority", core.system.time_authority, false);
    web_json_kv_str(response, "timeInitStatus", core.system.time_init_status, false);
    web_json_kv_str(response, "timeInitReason", core.system.time_init_reason, false);
    web_json_kv_bool(response, "timeValid", core.system.time_valid, false);
    web_json_kv_bool(response, "timeAuthoritative", core.system.time_authoritative, false);
    web_json_kv_u32(response, "timeSourceAgeMs", core.system.time_source_age_ms, false);
    web_json_kv_str(response, "degradedCode", core.system.degraded_code, false);
    web_json_kv_str(response, "degradedOwner", core.system.degraded_owner, false);
    web_json_kv_str(response, "degradedRecoveryHint", core.system.degraded_recovery_hint, include_sleep_state || include_safe_mode);
    if (include_sleep_state) {
        web_json_kv_bool(response, "sleeping", core.system.sleeping, false);
        web_json_kv_bool(response, "animating", core.system.animating, include_safe_mode);
    }
    if (include_safe_mode) {
        web_json_kv_bool(response, "safeMode", safe_mode_value, true);
    }
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_config_section(String &response,
                                        const WebApiStateBundle &bundle,
                                        bool last)
{
    response += "\"config\":{";
    if (bundle.cfg != nullptr) {
        web_json_kv_bool(response, "wifiConfigured", bundle.cfg->wifi_configured, false);
        web_json_kv_bool(response, "weatherConfigured", bundle.cfg->weather_configured, false);
        web_json_kv_bool(response, "authRequired", bundle.cfg->api_token_configured, false);
        web_json_kv_str(response, "wifiSsid", bundle.cfg->wifi_ssid, false);
        web_json_kv_str(response, "timezoneId", bundle.cfg->timezone_id, false);
        web_json_kv_bool(response, "filesystemReady", bundle.runtime.filesystem_ready, false);
        web_json_kv_bool(response, "filesystemAssetsReady", bundle.runtime.filesystem_assets_ready, false);
        web_json_kv_str(response, "filesystemStatus", bundle.runtime.filesystem_status, false);
        web_json_kv_str(response, "locationName", bundle.cfg->location_name, true);
    } else {
        web_json_kv_bool(response, "wifiConfigured", false, false);
        web_json_kv_bool(response, "weatherConfigured", false, false);
        web_json_kv_bool(response, "authRequired", false, false);
        web_json_kv_str(response, "wifiSsid", "", false);
        web_json_kv_str(response, "timezoneId", "", false);
        web_json_kv_bool(response, "filesystemReady", bundle.runtime.filesystem_ready, false);
        web_json_kv_bool(response, "filesystemAssetsReady", bundle.runtime.filesystem_assets_ready, false);
        web_json_kv_str(response, "filesystemStatus", bundle.runtime.filesystem_status, false);
        web_json_kv_str(response, "locationName", "", true);
    }
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_activity_section(String &response, const WebStateCoreSnapshot &core, bool last)
{
    response += "\"activity\":{";
    web_json_kv_u32(response, "steps", core.activity.steps, false);
    web_json_kv_u32(response, "goal", core.activity.goal, false);
    web_json_kv_u32(response, "goalPercent", core.activity.goal_percent, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_alarm_section(String &response, const WebApiStateBundle &bundle, bool last)
{
    response += "\"alarm\":{";
    web_json_kv_u32(response, "nextIndex", bundle.detail.alarm.next_alarm_index, false);
    web_json_kv_str(response, "nextTime", bundle.detail.alarm.next_time, false);
    web_json_kv_bool(response, "enabled", bundle.detail.alarm.enabled, false);
    web_json_kv_bool(response, "ringing", bundle.detail.alarm.ringing, false);
    web_json_kv_str(response, "label", bundle.core.labels.alarm_label, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_music_section(String &response, const WebApiStateBundle &bundle, bool last)
{
    response += "\"music\":{";
    web_json_kv_bool(response, "available", bundle.detail.music.available, false);
    web_json_kv_bool(response, "playing", bundle.detail.music.playing, false);
    web_json_kv_str(response, "state", bundle.detail.music.state, false);
    web_json_kv_str(response, "song", bundle.detail.music.song, false);
    web_json_kv_str(response, "label", bundle.core.labels.music_label, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_sensor_section(String &response,
                                        const WebStateSensorSnapshot &sensor,
                                        bool include_raw_axes,
                                        bool last)
{
    response += "\"sensor\":{";
    web_json_kv_bool(response, "online", sensor.online, false);
    web_json_kv_bool(response, "calibrated", sensor.calibrated, false);
    web_json_kv_bool(response, "staticNow", sensor.static_now, false);
    web_json_kv_str(response, "runtimeState", sensor.runtime_state, false);
    web_json_kv_str(response, "calibrationState", sensor.calibration_state, false);
    web_json_kv_u32(response, "quality", sensor.quality, false);
    web_json_kv_u32(response, "errorCode", sensor.error_code, false);
    web_json_kv_u32(response, "faultCount", sensor.fault_count, false);
    web_json_kv_u32(response, "reinitCount", sensor.reinit_count, false);
    web_json_kv_u32(response, "calibrationProgress", sensor.calibration_progress, false);
    if (include_raw_axes) {
        web_json_kv_i32(response, "ax", sensor.ax, false);
        web_json_kv_i32(response, "ay", sensor.ay, false);
        web_json_kv_i32(response, "az", sensor.az, false);
        web_json_kv_i32(response, "gx", sensor.gx, false);
        web_json_kv_i32(response, "gy", sensor.gy, false);
        web_json_kv_i32(response, "gz", sensor.gz, false);
        web_json_kv_i32(response, "accelNormMg", sensor.accel_norm_mg, false);
        web_json_kv_i32(response, "baselineMg", sensor.baseline_mg, false);
        web_json_kv_i32(response, "motionScore", sensor.motion_score, false);
    }
    web_json_kv_u32(response, "lastSampleMs", sensor.last_sample_ms, false);
    web_json_kv_u32(response, "stepsTotal", sensor.steps_total, include_raw_axes);
    if (include_raw_axes) {
        web_json_kv_f32(response, "pitchDeg", sensor.pitch_deg, 2, false);
        web_json_kv_f32(response, "rollDeg", sensor.roll_deg, 2, true);
    }
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_storage_section(String &response, const WebStateStorageSnapshot &storage, bool last)
{
    response += "\"storage\":{";
    web_json_kv_str(response, "backend", storage.backend, false);
    web_json_kv_str(response, "backendPhase", storage.backend_phase, false);
    web_json_kv_str(response, "commitState", storage.commit_state, false);
    web_json_kv_u32(response, "schemaVersion", storage.schema_version, false);
    web_json_kv_bool(response, "flashSupported", storage.flash_supported, false);
    web_json_kv_bool(response, "flashReady", storage.flash_ready, false);
    web_json_kv_bool(response, "migrationAttempted", storage.migration_attempted, false);
    web_json_kv_bool(response, "migrationOk", storage.migration_ok, false);
    web_json_kv_bool(response, "transactionActive", storage.transaction_active, false);
    web_json_kv_bool(response, "sleepFlushPending", storage.sleep_flush_pending, false);
    web_json_kv_str(response, "appStateBackend", storage.app_state_backend, false);
    web_json_kv_str(response, "deviceConfigBackend", storage.device_config_backend, false);
    web_json_kv_str(response, "appStateDurability", storage.app_state_durability, false);
    web_json_kv_str(response, "deviceConfigDurability", storage.device_config_durability, false);
    web_json_kv_bool(response, "appStatePowerLossGuaranteed", storage.app_state_power_loss_guaranteed, false);
    web_json_kv_bool(response, "deviceConfigPowerLossGuaranteed", storage.device_config_power_loss_guaranteed, false);
    web_json_kv_bool(response, "appStateMixedDurability", storage.app_state_mixed_durability, false);
    web_json_kv_u32(response, "appStateResetDomainObjectCount", storage.app_state_reset_domain_object_count, false);
    web_json_kv_u32(response, "appStateDurableObjectCount", storage.app_state_durable_object_count, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_diag_section(String &response, const WebStateDiagSnapshot &diag, bool include_safe_mode_fields, bool last)
{
    response += "\"diag\":{";
    if (include_safe_mode_fields) {
        web_json_kv_bool(response, "safeModeActive", diag.safe_mode_active, false);
        web_json_kv_bool(response, "safeModeCanClear", diag.safe_mode_can_clear, false);
        web_json_kv_str(response, "safeModeReason", diag.safe_mode_reason, false);
    }
    web_json_kv_u32(response, "consecutiveIncompleteBoots", diag.consecutive_incomplete_boots, false);
    web_json_kv_u32(response, "bootCount", diag.boot_count, false);
    web_json_kv_u32(response, "previousBootCount", diag.previous_boot_count, false);
    web_json_kv_bool(response, "hasLastFault", diag.has_last_fault, false);
    web_json_kv_str(response, "lastFaultName", diag.last_fault_name, false);
    web_json_kv_str(response, "lastFaultSeverity", diag.last_fault_severity, false);
    web_json_kv_u32(response, "lastFaultCount", diag.last_fault_count, false);
    web_json_kv_bool(response, "hasLastLog", diag.has_last_log, false);
    web_json_kv_str(response, "lastLogName", diag.last_log_name, false);
    web_json_kv_u32(response, "lastLogValue", diag.last_log_value, false);
    web_json_kv_u32(response, "lastLogAux", diag.last_log_aux, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_display_section(String &response, const WebStateDisplaySnapshot &display, bool last)
{
    response += "\"display\":{";
    web_json_kv_u32(response, "presentCount", display.present_count, false);
    web_json_kv_u32(response, "txFailCount", display.tx_fail_count, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_weather_section(String &response, const WebStateWeatherSnapshot &weather, bool include_updated_at, bool last)
{
    response += "\"weather\":{";
    web_json_kv_bool(response, "valid", weather.valid, false);
    web_json_kv_str(response, "location", weather.location, false);
    web_json_kv_str(response, "text", weather.text, false);
    web_json_kv_bool(response, "tlsVerified", weather.tls_verified, false);
    web_json_kv_bool(response, "tlsCaLoaded", weather.tls_ca_loaded, false);
    web_json_kv_str(response, "tlsMode", weather.tls_mode, false);
    web_json_kv_str(response, "syncStatus", weather.sync_status, false);
    web_json_kv_i32(response, "lastHttpStatus", weather.last_http_status, false);
    web_json_kv_f32(response, "temperatureC", (float)weather.temperature_tenths_c / 10.0f, 1, include_updated_at);
    if (include_updated_at) {
        web_json_kv_u32(response, "updatedAtMs", weather.updated_at_ms, true);
    }
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_summary_section(String &response, const WebStateCoreSnapshot &core, bool include_network_label, bool last)
{
    response += "\"summary\":{";
    web_json_kv_str(response, "headerTags", core.system.header_tags, false);
    web_json_kv_str(response, "networkLine", core.weather.network_line, false);
    web_json_kv_str(response, "networkSubline", core.weather.network_subline, false);
    web_json_kv_str(response, "sensorLabel", core.labels.sensor_label, false);
    web_json_kv_str(response, "storageLabel", core.labels.storage_label, false);
    web_json_kv_str(response, "diagLabel", core.labels.diag_label, include_network_label);
    if (include_network_label) {
        web_json_kv_str(response, "networkLabel", core.weather.network_label, true);
    }
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_terminal_section(String &response, const WebStateCoreSnapshot &core, bool last)
{
    response += "\"terminal\":{";
    web_json_kv_str(response, "systemFace", core.system.system_face, false);
    web_json_kv_str(response, "brightnessLabel", core.system.brightness_label, false);
    web_json_kv_str(response, "activityLabel", core.activity.activity_label, false);
    web_json_kv_str(response, "sensorLabel", core.labels.sensor_label, false);
    web_json_kv_str(response, "networkLabel", core.weather.network_label, false);
    web_json_kv_str(response, "storageLabel", core.labels.storage_label, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_overlay_section(String &response, const WebStateOverlaySnapshot &overlay, bool last)
{
    response += "\"overlay\":{";
    web_json_kv_bool(response, "active", overlay.active, false);
    web_json_kv_str(response, "text", overlay.text, false);
    web_json_kv_u32(response, "expireAtMs", overlay.expire_at_ms, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_startup_raw_section(String &response, const WebApiStateBundle &bundle, bool last)
{
    response += "\"startup\":{";
    if (bundle.runtime.has_startup_report) {
        web_json_kv_bool(response, "ok", bundle.runtime.startup_report.startup_ok, false);
        web_json_kv_bool(response, "degraded", bundle.runtime.startup_report.degraded, false);
        web_json_kv_bool(response, "fatalStopRequested", bundle.runtime.startup_report.fatal_stop_requested, false);
        web_json_kv_str(response, "failureStage", system_init_stage_name(bundle.runtime.startup_report.failure_stage), false);
        web_json_kv_str(response, "lastCompletedStage", system_init_stage_name(bundle.runtime.startup_report.last_completed_stage), false);
        web_json_kv_str(response, "recoveryStage", system_init_stage_name(bundle.runtime.startup_report.recovery_stage), false);
        web_json_kv_str(response, "fatalCode", system_fatal_code_name(bundle.runtime.startup_report.fatal_code), false);
        web_json_kv_str(response, "fatalPolicy", system_fatal_policy_name(bundle.runtime.startup_report.fatal_policy), true);
    } else {
        web_json_kv_bool(response, "ok", false, false);
        web_json_kv_bool(response, "degraded", false, false);
        web_json_kv_bool(response, "fatalStopRequested", false, false);
        web_json_kv_str(response, "failureStage", "UNKNOWN", false);
        web_json_kv_str(response, "lastCompletedStage", "UNKNOWN", false);
        web_json_kv_str(response, "recoveryStage", "UNKNOWN", false);
        web_json_kv_str(response, "fatalCode", "UNKNOWN", false);
        web_json_kv_str(response, "fatalPolicy", "UNKNOWN", true);
    }
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_queue_raw_section(String &response, const WebApiStateBundle &bundle, bool last)
{
    response += "\"queue\":{";
    web_json_kv_u32(response, "depth", bundle.detail.perf.action_queue_depth, false);
    web_json_kv_u32(response, "dropCount", bundle.detail.perf.action_queue_drop_count, true);
    response += "}";
    if (!last) {
        response += ",";
    }
}

static void append_state_section(String &response,
                                 const WebApiStateBundle &bundle,
                                 const WebStateSectionSpec &section,
                                 bool last)
{
    switch (section.id) {
        case WEB_STATE_SECTION_WIFI:
            append_state_wifi_section(response,
                                      bundle.core.wifi,
                                      (section.flags & WEB_STATE_SECTION_FLAG_WIFI_INCLUDE_RSSI) != 0U,
                                      last);
            break;
        case WEB_STATE_SECTION_SYSTEM:
            append_state_system_section(response,
                                        bundle.core,
                                        (section.flags & WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_APP_INIT_STAGE) != 0U,
                                        (section.flags & WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SLEEP_STATE) != 0U,
                                        (section.flags & WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SAFE_MODE) != 0U,
                                        bundle.detail.diag.safe_mode_active,
                                        last);
            break;
        case WEB_STATE_SECTION_CONFIG:
            append_state_config_section(response, bundle, last);
            break;
        case WEB_STATE_SECTION_ACTIVITY:
            append_state_activity_section(response, bundle.core, last);
            break;
        case WEB_STATE_SECTION_ALARM:
            append_state_alarm_section(response, bundle, last);
            break;
        case WEB_STATE_SECTION_MUSIC:
            append_state_music_section(response, bundle, last);
            break;
        case WEB_STATE_SECTION_SENSOR:
            append_state_sensor_section(response,
                                        bundle.detail.sensor,
                                        (section.flags & WEB_STATE_SECTION_FLAG_SENSOR_INCLUDE_RAW_AXES) != 0U,
                                        last);
            break;
        case WEB_STATE_SECTION_STORAGE:
            append_state_storage_section(response, bundle.detail.storage, last);
            break;
        case WEB_STATE_SECTION_DIAG:
            append_state_diag_section(response,
                                      bundle.detail.diag,
                                      (section.flags & WEB_STATE_SECTION_FLAG_DIAG_INCLUDE_SAFE_MODE_FIELDS) != 0U,
                                      last);
            break;
        case WEB_STATE_SECTION_DISPLAY:
            append_state_display_section(response, bundle.detail.display, last);
            break;
        case WEB_STATE_SECTION_WEATHER:
            append_state_weather_section(response,
                                         bundle.core.weather,
                                         (section.flags & WEB_STATE_SECTION_FLAG_WEATHER_INCLUDE_UPDATED_AT) != 0U,
                                         last);
            break;
        case WEB_STATE_SECTION_SUMMARY:
            append_state_summary_section(response,
                                         bundle.core,
                                         (section.flags & WEB_STATE_SECTION_FLAG_SUMMARY_INCLUDE_NETWORK_LABEL) != 0U,
                                         last);
            break;
        case WEB_STATE_SECTION_TERMINAL:
            append_state_terminal_section(response, bundle.core, last);
            break;
        case WEB_STATE_SECTION_OVERLAY:
            append_state_overlay_section(response, bundle.detail.overlay, last);
            break;
        case WEB_STATE_SECTION_PERF:
            append_perf_payload(response,
                                bundle.detail.perf,
                                (section.flags & WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY) != 0U);
            if (!last) {
                response += ",";
            }
            break;
        case WEB_STATE_SECTION_STARTUP_RAW:
            append_state_startup_raw_section(response, bundle, last);
            break;
        case WEB_STATE_SECTION_QUEUE_RAW:
            append_state_queue_raw_section(response, bundle, last);
            break;
        default:
            break;
    }
}

static void append_state_payload(String &response, const WebApiStateBundle &bundle, WebStatePayloadView view)
{
    const WebStateSectionSpec *schema = NULL;
    size_t section_count = 0U;
    String sections;

    schema = web_state_view_schema(view, &section_count);
    for (size_t i = 0U; i < section_count; ++i) {
        append_state_section(sections, bundle, schema[i], i + 1U == section_count);
    }

    response = "{";
    response += "\"ok\":true,";
    append_state_versions(response);
    web_json_kv_u32(response,
                    "stateRevision",
                    web_hash_bytes_fnv1a32(reinterpret_cast<const uint8_t *>(sections.c_str()), (size_t)sections.length()),
                    section_count == 0U);
    if (section_count != 0U) {
        response += ',';
        response += sections;
    }
    response += "}";
}

static void send_state_payload_response(AsyncWebServerRequest *request,
                                        WebStatePayloadView view,
                                        bool require_device_config,
                                        size_t reserve_hint)
{
    WebApiStateBundle bundle = {};
    String response;
    response.reserve(reserve_hint);

    if (!collect_api_state_bundle(&bundle, require_device_config)) {
        request->send(500, "application/json", "{\"ok\":false,\"message\":\"snapshot unavailable\"}");
        return;
    }

    append_state_payload(response, bundle, view);
    request->send(200, "application/json", response);
}

void web_send_state_detail_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_DETAIL, false, 6144U);
}

void web_send_state_perf_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_PERF, false, 4096U);
}

void web_send_state_raw_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_RAW, false, 4096U);
}

void web_send_state_aggregate_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_AGGREGATE, true, 6144U);
}
