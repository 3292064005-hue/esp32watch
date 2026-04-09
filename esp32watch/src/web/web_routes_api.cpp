#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "web/web_action_queue.h"
#include "web/web_state_bridge.h"
#include "web/web_json.h"
#include "web/web_wifi.h"
#include "web/web_server.h"
#include "web/web_api_config.h"
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

extern "C" {
#include "display_internal.h"
#include "key.h"
#include "app_command.h"
#include "main.h"
#include "services/device_config.h"
#include "services/network_sync_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "app_tuning.h"
#include "system_init.h"
}

static const char *kHexDigits = "0123456789ABCDEF";
static constexpr uint32_t kWebApiVersion = 4U;
static constexpr uint32_t kWebStateVersion = 4U;
static constexpr uint32_t kWebCommandCatalogVersion = 1U;

static bool parse_key_id(const char *key_str, KeyId *out_id)
{
    if (!key_str || !out_id) {
        return false;
    }

    if (strcmp(key_str, "up") == 0) {
        *out_id = KEY_ID_UP;
        return true;
    } else if (strcmp(key_str, "down") == 0) {
        *out_id = KEY_ID_DOWN;
        return true;
    } else if (strcmp(key_str, "ok") == 0) {
        *out_id = KEY_ID_OK;
        return true;
    } else if (strcmp(key_str, "back") == 0) {
        *out_id = KEY_ID_BACK;
        return true;
    }

    return false;
}

static bool parse_key_event(const char *event_str, KeyEventType *out_event)
{
    if (!event_str || !out_event) {
        return false;
    }

    if (strcmp(event_str, "short") == 0) {
        *out_event = KEY_EVENT_SHORT;
        return true;
    } else if (strcmp(event_str, "long") == 0) {
        *out_event = KEY_EVENT_LONG;
        return true;
    } else if (strcmp(event_str, "repeat") == 0) {
        *out_event = KEY_EVENT_REPEAT;
        return true;
    } else if (strcmp(event_str, "press") == 0) {
        *out_event = KEY_EVENT_PRESS;
        return true;
    } else if (strcmp(event_str, "release") == 0) {
        *out_event = KEY_EVENT_RELEASE;
        return true;
    }

    return false;
}

static bool parse_command_type(const char *cmd_str, AppCommandType *out_type)
{
    const AppCommandDescriptor *descriptor = app_command_describe_by_name(cmd_str);
    if (cmd_str == nullptr || out_type == nullptr || descriptor == nullptr || !descriptor->web_exposed) {
        return false;
    }
    *out_type = descriptor->type;
    return true;
}

static void send_json_error(AsyncWebServerRequest *request, int status, const char *message)
{
    String response = "{\"ok\":false,\"message\":\"";
    web_json_escape_append(response, message != nullptr ? message : "error");
    response += "\"}";
    request->send(status, "application/json", response);
}

static bool read_json_body(AsyncWebServerRequest *request, String &body)
{
    if (request == nullptr) {
        return false;
    }

    if (request->_tempObject != nullptr) {
        body = static_cast<const char *>(request->_tempObject);
        return body.length() > 0;
    }

    body = request->arg("plain");
    if (body.length() > 0) {
        return true;
    }

    if (request->hasParam("body", true)) {
        body = request->getParam("body", true)->value();
        return body.length() > 0;
    }

    return false;
}

template <size_t N>
static bool parse_json_body(AsyncWebServerRequest *request, StaticJsonDocument<N> &doc, String *out_body = nullptr)
{
    String body;

    if (!read_json_body(request, body)) {
        return false;
    }
    if (body.length() > WEB_API_MAX_BODY_BYTES) {
        return false;
    }
    if (deserializeJson(doc, body)) {
        return false;
    }
    if (out_body != nullptr) {
        *out_body = body;
    }
    return true;
}

static bool read_form_field(AsyncWebServerRequest *request, const char *name, String &value)
{
    if (request == nullptr || name == nullptr) {
        return false;
    }

    if (!request->hasParam(name, true)) {
        if (!request->hasParam(name)) {
            return false;
        }
        value = request->getParam(name)->value();
        return true;
    }

    value = request->getParam(name, true)->value();
    return true;
}

static bool parse_strict_float_field(const String &value, float *out_value)
{
    char *end_ptr = nullptr;
    double parsed;

    if (out_value == nullptr || value.length() == 0) {
        return false;
    }

    parsed = strtod(value.c_str(), &end_ptr);
    if (end_ptr == value.c_str() || end_ptr == nullptr) {
        return false;
    }
    while (*end_ptr != '\0' && isspace((unsigned char)(*end_ptr)) != 0) {
        ++end_ptr;
    }
    if (*end_ptr != '\0') {
        return false;
    }

    *out_value = (float)parsed;
    return std::isfinite((double)(*out_value));
}

static bool parse_strict_u32_field(const String &value, uint32_t *out_value)
{
    const char *cursor = value.c_str();
    char *end_ptr = nullptr;
    unsigned long parsed = 0UL;

    if (out_value == nullptr || value.length() == 0) {
        return false;
    }

    while (*cursor != '\0' && isspace((unsigned char)(*cursor)) != 0) {
        ++cursor;
    }
    if (*cursor == '-' || *cursor == '+') {
        return false;
    }

    parsed = strtoul(cursor, &end_ptr, 10);
    if (end_ptr == cursor || end_ptr == nullptr) {
        return false;
    }
    while (*end_ptr != '\0' && isspace((unsigned char)(*end_ptr)) != 0) {
        ++end_ptr;
    }
    if (*end_ptr != '\0') {
        return false;
    }
    if (parsed > 0xFFFFFFFFUL) {
        return false;
    }

    *out_value = (uint32_t)parsed;
    return true;
}

template <typename TRoot>
static bool read_json_string_field(const TRoot &root, const char *name, String &value)
{
    JsonVariant field = root[name];
    if (field.isNull() || !field.is<const char *>()) {
        return false;
    }
    value = field.as<const char *>();
    return true;
}

template <typename TRoot>
static bool read_json_u32_field(const TRoot &root, const char *name, uint32_t *value)
{
    JsonVariant field = root[name];
    if (value == nullptr || field.isNull()) {
        return false;
    }
    if (field.is<int>()) {
        int signed_value = field.as<int>();
        if (signed_value < 0) {
            return false;
        }
        *value = (uint32_t)signed_value;
        return true;
    }
    if (field.is<unsigned int>()) {
        *value = (uint32_t)field.as<unsigned int>();
        return true;
    }
    if (field.is<uint32_t>()) {
        *value = field.as<uint32_t>();
        return true;
    }
    if (field.is<unsigned long>()) {
        unsigned long raw = field.as<unsigned long>();
        if (raw > 0xFFFFFFFFUL) {
            return false;
        }
        *value = (uint32_t)raw;
        return true;
    }
    return false;
}

template <typename TRoot>
static bool read_json_float_field(const TRoot &root, const char *name, float *value)
{
    JsonVariant field = root[name];
    if (value == nullptr || field.isNull()) {
        return false;
    }
    if (!field.is<float>() && !field.is<double>() && !field.is<int>()) {
        return false;
    }
    *value = field.as<float>();
    return std::isfinite((double)(*value));
}

static void capture_request_body(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    if (request == nullptr || data == nullptr || len == 0) {
        return;
    }

    if (total > WEB_API_MAX_BODY_BYTES) {
        return;
    }

    if (index == 0 && request->_tempObject == nullptr) {
        request->_tempObject = calloc(total + 1U, sizeof(uint8_t));
        if (request->_tempObject == nullptr) {
            return;
        }
    }

    if (request->_tempObject != nullptr) {
        uint8_t *buffer = static_cast<uint8_t *>(request->_tempObject);
        memcpy(buffer + index, data, len);
    }
}

static void append_hex_byte(String &out, uint8_t value)
{
    out += kHexDigits[(value >> 4) & 0x0F];
    out += kHexDigits[value & 0x0F];
}

static bool auth_token_from_request(AsyncWebServerRequest *request, String &token)
{
    if (request == nullptr) {
        return false;
    }
    if (request->hasHeader("X-Auth-Token")) {
        token = request->getHeader("X-Auth-Token")->value();
        if (token.length() > 0) {
            return true;
        }
    }
    if (read_form_field(request, "token", token)) {
        return token.length() > 0;
    }
    if (request->hasParam("token")) {
        token = request->getParam("token")->value();
        return token.length() > 0;
    }

    StaticJsonDocument<128> doc;
    if (parse_json_body(request, doc) && read_json_string_field(doc, "token", token)) {
        return token.length() > 0;
    }
    return false;
}

static bool request_is_authorized(AsyncWebServerRequest *request)
{
    String token;
    if (!device_config_has_api_token()) {
        return true;
    }
    if (!auth_token_from_request(request, token)) {
        return false;
    }
    return device_config_authenticate_token(token.c_str());
}

static bool control_is_locked_in_provisioning_ap(void)
{
    return !device_config_has_api_token() && web_wifi_access_point_active() && !web_wifi_connected();
}

static bool require_mutation_auth(AsyncWebServerRequest *request)
{
    if (request_is_authorized(request)) {
        return true;
    }
    request->send(401, "application/json", "{\"ok\":false,\"message\":\"unauthorized\"}");
    return false;
}

static bool require_control_auth(AsyncWebServerRequest *request)
{
    if (!request_is_authorized(request)) {
        request->send(401, "application/json", "{\"ok\":false,\"message\":\"unauthorized\"}");
        return false;
    }
    if (control_is_locked_in_provisioning_ap()) {
        request->send(403, "application/json", "{\"ok\":false,\"message\":\"control locked until API token is configured or STA link is active\"}");
        return false;
    }
    return true;
}

static void append_action_status_payload(String &response, const WebActionStatusSnapshot &status, bool append_ok_prefix)
{
    if (append_ok_prefix) {
        response += "\"ok\":true,";
    }
    web_json_kv_u32(response, "actionId", status.id, false);
    web_json_kv_u32(response, "requestId", status.id, false);
    web_json_kv_str(response, "actionType", web_action_type_name(status.type), false);
    web_json_kv_str(response, "status", web_action_status_name(status.status), false);
    web_json_kv_str(response, "state", web_action_status_name(status.status), false);
    web_json_kv_u32(response, "acceptedAtMs", status.accepted_at_ms, false);
    web_json_kv_u32(response, "startedAtMs", status.started_at_ms, false);
    web_json_kv_u32(response, "completedAtMs", status.completed_at_ms, false);
    web_json_kv_str(response, "message", status.message, false);
    if (status.type == WEB_ACTION_COMMAND) {
        web_json_kv_bool(response, "commandOk", status.command_ok, false);
        web_json_kv_str(response, "commandCode", app_command_result_code_name(status.command_code), true);
    } else {
        web_json_kv_str(response, "commandCode", "N/A", true);
    }
}

static void send_queue_ack(AsyncWebServerRequest *request, uint32_t action_id, WebActionType type)
{
    String response = "{\"ok\":true,\"message\":\"accepted\",";
    web_json_kv_u32(response, "actionId", action_id, false);
    web_json_kv_u32(response, "requestId", action_id, false);
    web_json_kv_str(response, "actionType", web_action_type_name(type), false);
    response += "\"trackPath\":\"/api/actions/status?id=";
    response += action_id;
    response += "\",";
    web_json_kv_u32(response, "queueDepth", web_action_queue_depth(), true);
    response += "}";
    request->send(200, "application/json", response);
}

static void append_command_catalog(String &response)
{
    size_t i;
    response += "\"commandCatalogVersion\":";
    response += kWebCommandCatalogVersion;
    response += ",\"commands\":[";
    for (i = 0; i < app_command_catalog_count(); ++i) {
        const AppCommandDescriptor *descriptor = app_command_catalog_at(i);
        if (descriptor == nullptr || !descriptor->web_exposed) {
            continue;
        }
        if (response[response.length() - 1U] != '[') {
            response += ',';
        }
        response += '{';
        web_json_kv_str(response, "type", descriptor->wire_name, false);
        web_json_kv_bool(response, "webExposed", descriptor->web_exposed, true);
        response += '}';
    }
    response += ']';
}

static void append_capabilities(String &response)
{
    response += "\"capabilities\":{";
    web_json_kv_bool(response, "configProvisioning", true, false);
    web_json_kv_bool(response, "securedProvisioningAp", true, false);
    web_json_kv_bool(response, "authToken", true, false);
    web_json_kv_bool(response, "overlayControl", true, false);
    web_json_kv_bool(response, "trackedMutations", true, false);
    web_json_kv_bool(response, "stateSummary", true, false);
    web_json_kv_bool(response, "stateDetail", true, false);
    web_json_kv_bool(response, "stateRaw", true, false);
    web_json_kv_bool(response, "controlLockInProvisioningAp", true, true);
    response += "}";
}

static void send_meta_response(AsyncWebServerRequest *request)
{
    NetworkSyncSnapshot network = {};
    String response = "{";

    (void)network_sync_service_get_snapshot(&network);

    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", kWebApiVersion, false);
    web_json_kv_u32(response, "stateVersion", kWebStateVersion, false);
    web_json_kv_u32(response, "storageSchemaVersion", APP_STORAGE_VERSION, false);
    web_json_kv_bool(response, "flashStorageSupported", storage_service_flash_supported(), false);
    web_json_kv_bool(response, "flashStorageReady", storage_service_is_flash_backend_ready(), false);
    web_json_kv_bool(response, "storageMigrationAttempted", storage_service_was_migration_attempted(), false);
    web_json_kv_bool(response, "storageMigrationOk", storage_service_last_migration_ok(), false);
    web_json_kv_bool(response, "authRequired", device_config_has_api_token(), false);
    web_json_kv_bool(response, "controlLockedInProvisioningAp", control_is_locked_in_provisioning_ap(), false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
    web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
    web_json_kv_bool(response, "weatherTlsVerified", network.weather_tls_verified, false);
    web_json_kv_bool(response, "weatherCaLoaded", network.weather_ca_loaded, false);
    web_json_kv_str(response, "weatherTlsMode", network.weather_tls_mode, false);
    web_json_kv_str(response, "weatherSyncStatus", network.weather_status, false);
    web_json_kv_bool(response, "weatherLastAttemptOk", network.weather_last_attempt_ok, false);
    web_json_kv_i32(response, "weatherLastHttpStatus", network.last_weather_http_status, false);
    {
        SystemStartupReport startup = {};
        if (system_get_startup_report(&startup)) {
            response += "\"startup\":{";
            web_json_kv_bool(response, "ok", startup.startup_ok, false);
            web_json_kv_bool(response, "degraded", startup.degraded, false);
            web_json_kv_bool(response, "fatalStopRequested", startup.fatal_stop_requested, false);
            web_json_kv_str(response, "failureStage", system_init_stage_name(startup.failure_stage), false);
            web_json_kv_str(response, "lastCompletedStage", system_init_stage_name(startup.last_completed_stage), false);
            web_json_kv_str(response, "recoveryStage", system_init_stage_name(startup.recovery_stage), false);
            web_json_kv_str(response, "fatalCode", system_fatal_code_name(startup.fatal_code), false);
            web_json_kv_str(response, "fatalPolicy", system_fatal_policy_name(startup.fatal_policy), true);
            response += "},";
        }
    }
    append_command_catalog(response);
    response += ",";
    append_capabilities(response);
    response += "}";
    request->send(200, "application/json", response);
}

static void send_device_config_response(AsyncWebServerRequest *request)
{
    DeviceConfigSnapshot cfg;
    NetworkSyncSnapshot network = {};
    String response = "{";

    device_config_init();
    (void)device_config_get(&cfg);
    (void)network_sync_service_get_snapshot(&network);

    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", kWebApiVersion, false);
    web_json_kv_u32(response, "stateVersion", kWebStateVersion, false);
    response += "\"config\":{";
    web_json_kv_u32(response, "version", cfg.version, false);
    web_json_kv_bool(response, "wifiConfigured", cfg.wifi_configured, false);
    web_json_kv_bool(response, "weatherConfigured", cfg.weather_configured, false);
    web_json_kv_bool(response, "apiTokenConfigured", cfg.api_token_configured, false);
    web_json_kv_bool(response, "authRequired", cfg.api_token_configured, false);
    web_json_kv_str(response, "wifiSsid", cfg.wifi_ssid, false);
    web_json_kv_str(response, "timezonePosix", cfg.timezone_posix, false);
    web_json_kv_str(response, "timezoneId", cfg.timezone_id, false);
    web_json_kv_f32(response, "latitude", cfg.latitude, 6, false);
    web_json_kv_f32(response, "longitude", cfg.longitude, 6, false);
    web_json_kv_str(response, "locationName", cfg.location_name, false);
    web_json_kv_str(response, "wifiMode", web_wifi_mode_name(), false);
    web_json_kv_str(response, "wifiStatus", web_wifi_status_name(), false);
    web_json_kv_bool(response, "apActive", web_wifi_access_point_active(), false);
    web_json_kv_str(response, "apSsid", web_wifi_access_point_ssid(), false);
    web_json_kv_bool(response, "controlLockedInProvisioningAp", control_is_locked_in_provisioning_ap(), false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
    web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
    web_json_kv_bool(response, "weatherTlsVerified", network.weather_tls_verified, false);
    web_json_kv_bool(response, "weatherCaLoaded", network.weather_ca_loaded, false);
    web_json_kv_str(response, "weatherTlsMode", network.weather_tls_mode, false);
    web_json_kv_str(response, "weatherSyncStatus", network.weather_status, false);
    web_json_kv_i32(response, "weatherLastHttpStatus", network.last_weather_http_status, false);
    web_json_kv_str(response, "ip", web_wifi_ip_string(), true);
    response += "},";
    append_capabilities(response);
    response += "}";

    request->send(200, "application/json", response);
}

static void send_action_result_response(AsyncWebServerRequest *request, uint32_t action_id)
{
    WebActionStatusSnapshot result = {};
    String response = "{";

    if (!web_action_status_lookup(action_id, &result)) {
        send_json_error(request, 404, "action id not found");
        return;
    }

    append_action_status_payload(response, result, true);
    response += "}";
    request->send(200, "application/json", response);
}

static void send_latest_action_response(AsyncWebServerRequest *request)
{
    WebActionStatusSnapshot result = {};
    String response = "{";

    if (!web_action_status_latest(&result)) {
        send_json_error(request, 404, "no tracked actions");
        return;
    }

    append_action_status_payload(response, result, true);
    response += "}";
    request->send(200, "application/json", response);
}

static void send_state_detail_response(AsyncWebServerRequest *request)
{
    WebStateCoreSnapshot core;
    WebStateDetailSnapshot detail;
    String response;
    response.reserve(3072);

    if (!web_state_core_collect(&core) || !web_state_detail_collect(&detail)) {
        request->send(500, "application/json", "{\"ok\":false,\"message\":\"snapshot unavailable\"}");
        return;
    }

    response = "{";
    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", kWebApiVersion, false);
    web_json_kv_u32(response, "stateVersion", kWebStateVersion, false);
    response += "\"activity\":{";
    web_json_kv_u32(response, "steps", core.activity.steps, false);
    web_json_kv_u32(response, "goal", core.activity.goal, false);
    web_json_kv_u32(response, "goalPercent", core.activity.goal_percent, true);
    response += "},";
    response += "\"alarm\":{";
    web_json_kv_u32(response, "nextIndex", detail.alarm.next_alarm_index, false);
    web_json_kv_str(response, "nextTime", detail.alarm.next_time, false);
    web_json_kv_bool(response, "enabled", detail.alarm.enabled, false);
    web_json_kv_bool(response, "ringing", detail.alarm.ringing, false);
    web_json_kv_str(response, "label", core.labels.alarm_label, true);
    response += "},";
    response += "\"music\":{";
    web_json_kv_bool(response, "available", detail.music.available, false);
    web_json_kv_bool(response, "playing", detail.music.playing, false);
    web_json_kv_str(response, "state", detail.music.state, false);
    web_json_kv_str(response, "song", detail.music.song, false);
    web_json_kv_str(response, "label", core.labels.music_label, true);
    response += "},";
    response += "\"sensor\":{";
    web_json_kv_bool(response, "online", detail.sensor.online, false);
    web_json_kv_bool(response, "calibrated", detail.sensor.calibrated, false);
    web_json_kv_bool(response, "staticNow", detail.sensor.static_now, false);
    web_json_kv_str(response, "runtimeState", detail.sensor.runtime_state, false);
    web_json_kv_str(response, "calibrationState", detail.sensor.calibration_state, false);
    web_json_kv_u32(response, "quality", detail.sensor.quality, false);
    web_json_kv_u32(response, "errorCode", detail.sensor.error_code, false);
    web_json_kv_u32(response, "faultCount", detail.sensor.fault_count, false);
    web_json_kv_u32(response, "reinitCount", detail.sensor.reinit_count, false);
    web_json_kv_u32(response, "calibrationProgress", detail.sensor.calibration_progress, false);
    web_json_kv_u32(response, "lastSampleMs", detail.sensor.last_sample_ms, false);
    web_json_kv_u32(response, "stepsTotal", detail.sensor.steps_total, true);
    response += "},";
    response += "\"storage\":{";
    web_json_kv_str(response, "backend", detail.storage.backend, false);
    web_json_kv_str(response, "backendPhase", detail.storage.backend_phase, false);
    web_json_kv_str(response, "commitState", detail.storage.commit_state, false);
    web_json_kv_u32(response, "schemaVersion", detail.storage.schema_version, false);
    web_json_kv_bool(response, "flashSupported", detail.storage.flash_supported, false);
    web_json_kv_bool(response, "flashReady", detail.storage.flash_ready, false);
    web_json_kv_bool(response, "migrationAttempted", detail.storage.migration_attempted, false);
    web_json_kv_bool(response, "migrationOk", detail.storage.migration_ok, false);
    web_json_kv_bool(response, "transactionActive", detail.storage.transaction_active, false);
    web_json_kv_bool(response, "sleepFlushPending", detail.storage.sleep_flush_pending, true);
    response += "},";
    response += "\"diag\":{";
    web_json_kv_bool(response, "safeModeActive", detail.diag.safe_mode_active, false);
    web_json_kv_bool(response, "safeModeCanClear", detail.diag.safe_mode_can_clear, false);
    web_json_kv_str(response, "safeModeReason", detail.diag.safe_mode_reason, false);
    web_json_kv_bool(response, "hasLastFault", detail.diag.has_last_fault, false);
    web_json_kv_str(response, "lastFaultName", detail.diag.last_fault_name, false);
    web_json_kv_str(response, "lastFaultSeverity", detail.diag.last_fault_severity, false);
    web_json_kv_u32(response, "lastFaultCount", detail.diag.last_fault_count, false);
    web_json_kv_bool(response, "hasLastLog", detail.diag.has_last_log, false);
    web_json_kv_str(response, "lastLogName", detail.diag.last_log_name, false);
    web_json_kv_u32(response, "lastLogValue", detail.diag.last_log_value, false);
    web_json_kv_u32(response, "lastLogAux", detail.diag.last_log_aux, true);
    response += "},";
    response += "\"display\":{";
    web_json_kv_u32(response, "presentCount", detail.display.present_count, false);
    web_json_kv_u32(response, "txFailCount", detail.display.tx_fail_count, true);
    response += "},";
    response += "\"weather\":{";
    web_json_kv_bool(response, "valid", core.weather.valid, false);
    web_json_kv_str(response, "location", core.weather.location, false);
    web_json_kv_str(response, "text", core.weather.text, false);
    web_json_kv_bool(response, "tlsVerified", core.weather.tls_verified, false);
    web_json_kv_bool(response, "tlsCaLoaded", core.weather.tls_ca_loaded, false);
    web_json_kv_str(response, "tlsMode", core.weather.tls_mode, false);
    web_json_kv_str(response, "syncStatus", core.weather.sync_status, false);
    web_json_kv_i32(response, "lastHttpStatus", core.weather.last_http_status, false);
    web_json_kv_f32(response, "temperatureC", (float)core.weather.temperature_tenths_c / 10.0f, 1, false);
    web_json_kv_u32(response, "updatedAtMs", core.weather.updated_at_ms, true);
    response += "},";
    response += "\"terminal\":{";
    web_json_kv_str(response, "systemFace", core.system.system_face, false);
    web_json_kv_str(response, "brightnessLabel", core.system.brightness_label, false);
    web_json_kv_str(response, "activityLabel", core.activity.activity_label, false);
    web_json_kv_str(response, "sensorLabel", core.labels.sensor_label, false);
    web_json_kv_str(response, "networkLabel", core.weather.network_label, false);
    web_json_kv_str(response, "storageLabel", core.labels.storage_label, true);
    response += "},";
    response += "\"overlay\":{";
    web_json_kv_bool(response, "active", detail.overlay.active, false);
    web_json_kv_str(response, "text", detail.overlay.text, false);
    web_json_kv_u32(response, "expireAtMs", detail.overlay.expire_at_ms, true);
    response += "}";
    response += "}";
    request->send(200, "application/json", response);
}

static void send_state_raw_response(AsyncWebServerRequest *request)
{
    WebStateDetailSnapshot detail;
    SystemStartupReport startup = {};
    String response;
    response.reserve(2048);

    if (!web_state_detail_collect(&detail)) {
        request->send(500, "application/json", "{\"ok\":false,\"message\":\"snapshot unavailable\"}");
        return;
    }

    response = "{";
    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", kWebApiVersion, false);
    web_json_kv_u32(response, "stateVersion", kWebStateVersion, false);
    response += "\"startup\":{";
    if (system_get_startup_report(&startup)) {
        web_json_kv_bool(response, "ok", startup.startup_ok, false);
        web_json_kv_bool(response, "degraded", startup.degraded, false);
        web_json_kv_bool(response, "fatalStopRequested", startup.fatal_stop_requested, false);
        web_json_kv_str(response, "failureStage", system_init_stage_name(startup.failure_stage), false);
        web_json_kv_str(response, "lastCompletedStage", system_init_stage_name(startup.last_completed_stage), false);
        web_json_kv_str(response, "recoveryStage", system_init_stage_name(startup.recovery_stage), false);
        web_json_kv_str(response, "fatalCode", system_fatal_code_name(startup.fatal_code), false);
        web_json_kv_str(response, "fatalPolicy", system_fatal_policy_name(startup.fatal_policy), true);
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
    response += "},";
    response += "\"queue\":{";
    web_json_kv_u32(response, "depth", web_action_queue_depth(), false);
    web_json_kv_u32(response, "dropCount", web_action_queue_drop_count(), true);
    response += "},";
    response += "\"sensor\":{";
    web_json_kv_i32(response, "ax", detail.sensor.ax, false);
    web_json_kv_i32(response, "ay", detail.sensor.ay, false);
    web_json_kv_i32(response, "az", detail.sensor.az, false);
    web_json_kv_i32(response, "gx", detail.sensor.gx, false);
    web_json_kv_i32(response, "gy", detail.sensor.gy, false);
    web_json_kv_i32(response, "gz", detail.sensor.gz, false);
    web_json_kv_i32(response, "accelNormMg", detail.sensor.accel_norm_mg, false);
    web_json_kv_i32(response, "baselineMg", detail.sensor.baseline_mg, false);
    web_json_kv_i32(response, "motionScore", detail.sensor.motion_score, false);
    web_json_kv_f32(response, "pitchDeg", detail.sensor.pitch_deg, 2, false);
    web_json_kv_f32(response, "rollDeg", detail.sensor.roll_deg, 2, true);
    response += "},";
    response += "\"storage\":{";
    web_json_kv_str(response, "backend", detail.storage.backend, false);
    web_json_kv_str(response, "backendPhase", detail.storage.backend_phase, false);
    web_json_kv_str(response, "commitState", detail.storage.commit_state, false);
    web_json_kv_bool(response, "transactionActive", detail.storage.transaction_active, false);
    web_json_kv_bool(response, "sleepFlushPending", detail.storage.sleep_flush_pending, true);
    response += "},";
    response += "\"display\":{";
    web_json_kv_u32(response, "presentCount", detail.display.present_count, false);
    web_json_kv_u32(response, "txFailCount", detail.display.tx_fail_count, true);
    response += "}";
    response += "}";
    request->send(200, "application/json", response);
}

void web_register_api_routes(AsyncWebServer &server)
{
    server.on("/api/health", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response = "{";
        response += "\"ok\":true,";
        response += "\"wifiConnected\":";
        response += web_wifi_connected() ? "true" : "false";
        response += ",\"ip\":\"";
        response += web_wifi_ip_string();
        response += "\",\"uptimeMs\":";
        response += (uint64_t)millis();
        response += "}";
        request->send(200, "application/json", response);
    });

    server.on("/api/meta", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_meta_response(request);
    });

    server.on("/api/actions/catalog", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response = "{";
        response += "\"ok\":true,";
        append_command_catalog(response);
        response += "}";
        request->send(200, "application/json", response);
    });

    server.on("/api/actions/latest", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_latest_action_response(request);
    });

    server.on("/api/actions/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String id_str;
        uint32_t action_id = 0U;
        if (!read_form_field(request, "id", id_str) || !parse_strict_u32_field(id_str, &action_id) || action_id == 0U) {
            send_json_error(request, 400, "missing or invalid action id");
            return;
        }
        send_action_result_response(request, action_id);
    });

    server.on("/api/action/result", HTTP_GET, [](AsyncWebServerRequest *request) {
        String id_str;
        uint32_t request_id = 0U;
        if (!read_form_field(request, "id", id_str) && !request->hasParam("id")) {
            send_json_error(request, 400, "missing request id");
            return;
        }
        if (request->hasParam("id")) {
            id_str = request->getParam("id")->value();
        }
        if (!parse_strict_u32_field(id_str, &request_id) || request_id == 0U) {
            send_json_error(request, 400, "invalid request id");
            return;
        }
        send_action_result_response(request, request_id);
    });

    server.on("/api/config/device", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_device_config_response(request);
    });

    server.on("/api/config/device", HTTP_POST, [](AsyncWebServerRequest *request) {
        DeviceConfigSnapshot current_cfg;
        DeviceConfigUpdate update = {};
        StaticJsonDocument<512> json_doc;
        String raw_body;
        String ssid;
        String password;
        String timezone_posix;
        String timezone_id;
        String location_name;
        String api_token;
        char current_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
        float latitude = 0.0f;
        float longitude = 0.0f;
        bool has_json_body = false;
        bool json_body_valid = false;
        bool has_ssid_field = false;
        bool has_password_field = false;
        bool has_timezone_posix_field = false;
        bool has_timezone_id_field = false;
        bool has_latitude_field = false;
        bool has_longitude_field = false;
        bool has_location_field = false;
        bool has_token_field = false;
        bool wifi_saved = false;
        bool network_saved = false;
        bool token_saved = false;

        if (!require_mutation_auth(request)) {
            return;
        }

        device_config_init();
        (void)device_config_get(&current_cfg);
        (void)device_config_get_wifi_password(current_password, sizeof(current_password));
        latitude = current_cfg.latitude;
        longitude = current_cfg.longitude;

        has_json_body = read_json_body(request, raw_body);
        if (has_json_body) {
            json_body_valid = parse_json_body(request, json_doc);
            if (!json_body_valid) {
                send_json_error(request, 400, "invalid json body");
                return;
            }
            has_ssid_field = read_json_string_field(json_doc, "ssid", ssid);
            has_password_field = read_json_string_field(json_doc, "password", password);
            has_timezone_posix_field = read_json_string_field(json_doc, "timezonePosix", timezone_posix);
            has_timezone_id_field = read_json_string_field(json_doc, "timezoneId", timezone_id);
            has_location_field = read_json_string_field(json_doc, "locationName", location_name);
            has_token_field = read_json_string_field(json_doc, "apiToken", api_token);
            has_latitude_field = read_json_float_field(json_doc, "latitude", &latitude);
            has_longitude_field = read_json_float_field(json_doc, "longitude", &longitude);
        } else {
            String latitude_str;
            String longitude_str;
            has_ssid_field = read_form_field(request, "ssid", ssid);
            has_password_field = read_form_field(request, "password", password);
            has_timezone_posix_field = read_form_field(request, "timezonePosix", timezone_posix);
            has_timezone_id_field = read_form_field(request, "timezoneId", timezone_id);
            has_latitude_field = read_form_field(request, "latitude", latitude_str);
            has_longitude_field = read_form_field(request, "longitude", longitude_str);
            has_location_field = read_form_field(request, "locationName", location_name);
            has_token_field = read_form_field(request, "apiToken", api_token);

            if (has_latitude_field && !parse_strict_float_field(latitude_str, &latitude)) {
                send_json_error(request, 400, "invalid latitude");
                return;
            }
            if (has_longitude_field && !parse_strict_float_field(longitude_str, &longitude)) {
                send_json_error(request, 400, "invalid longitude");
                return;
            }
        }

        if (!has_ssid_field && !has_password_field &&
            !has_timezone_posix_field && !has_timezone_id_field &&
            !has_latitude_field && !has_longitude_field && !has_location_field &&
            !has_token_field) {
            send_json_error(request, 400, "no config fields");
            return;
        }

        if (has_ssid_field || has_password_field) {
            update.set_wifi = true;
            strncpy(update.wifi_ssid,
                    has_ssid_field ? ssid.c_str() : current_cfg.wifi_ssid,
                    sizeof(update.wifi_ssid) - 1U);
            strncpy(update.wifi_password,
                    has_password_field ? password.c_str() : current_password,
                    sizeof(update.wifi_password) - 1U);
        }

        if (has_timezone_posix_field || has_timezone_id_field || has_latitude_field || has_longitude_field || has_location_field) {
            update.set_network = true;
            strncpy(update.timezone_posix,
                    has_timezone_posix_field ? timezone_posix.c_str() : current_cfg.timezone_posix,
                    sizeof(update.timezone_posix) - 1U);
            strncpy(update.timezone_id,
                    has_timezone_id_field ? timezone_id.c_str() : current_cfg.timezone_id,
                    sizeof(update.timezone_id) - 1U);
            strncpy(update.location_name,
                    has_location_field ? location_name.c_str() : current_cfg.location_name,
                    sizeof(update.location_name) - 1U);
            update.latitude = latitude;
            update.longitude = longitude;
        }

        if (has_token_field) {
            update.set_api_token = true;
            strncpy(update.api_token, api_token.c_str(), sizeof(update.api_token) - 1U);
        }

        if (!device_config_apply_update(&update)) {
            send_json_error(request, 400, "invalid device config");
            return;
        }

        wifi_saved = update.set_wifi;
        network_saved = update.set_network;
        token_saved = update.set_api_token;

        if (wifi_saved) {
            (void)web_wifi_reconfigure();
        }
        if (wifi_saved || network_saved) {
            network_sync_service_on_config_changed();
        }

        String response = "{";
        response += "\"ok\":true,";
        web_json_kv_bool(response, "wifiSaved", wifi_saved, false);
        web_json_kv_bool(response, "networkSaved", network_saved, false);
        web_json_kv_bool(response, "tokenSaved", token_saved, false);
        web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
        web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
        web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), true);
        response += "}";
        request->send(200, "application/json", response);
    }, nullptr, capture_request_body);

    server.on("/api/config/device/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!require_mutation_auth(request)) {
            return;
        }
        if (!device_config_restore_defaults()) {
            request->send(500, "application/json", "{\"ok\":false,\"message\":\"restore failed\"}");
            return;
        }
        (void)web_wifi_reconfigure();
        network_sync_service_on_config_changed();
        request->send(200, "application/json", "{\"ok\":true,\"message\":\"defaults restored\"}");
    }, nullptr, capture_request_body);

    server.on("/api/state/summary", HTTP_GET, [](AsyncWebServerRequest *request) {
        WebStateCoreSnapshot core;
        if (!web_state_core_collect(&core)) {
            request->send(500, "application/json", "{\"ok\":false,\"message\":\"snapshot unavailable\"}");
            return;
        }
        String response = "{";
        response += "\"ok\":true,";
        web_json_kv_u32(response, "apiVersion", kWebApiVersion, false);
        web_json_kv_u32(response, "stateVersion", kWebStateVersion, false);
        response += "\"wifi\":{";
        web_json_kv_bool(response, "connected", core.wifi.connected, false);
        web_json_kv_bool(response, "provisioningApActive", core.wifi.provisioning_ap_active, false);
        web_json_kv_str(response, "provisioningApSsid", core.wifi.provisioning_ap_ssid, false);
        web_json_kv_str(response, "status", core.wifi.status, false);
        web_json_kv_str(response, "mode", core.wifi.mode, false);
        web_json_kv_str(response, "ip", core.wifi.ip, true);
        response += "},";
        response += "\"system\":{";
        web_json_kv_bool(response, "appReady", core.system.app_ready, false);
        web_json_kv_bool(response, "appDegraded", core.system.app_degraded, false);
        web_json_kv_bool(response, "startupOk", core.system.startup_ok, false);
        web_json_kv_bool(response, "startupDegraded", core.system.startup_degraded, false);
        web_json_kv_bool(response, "fatalStopRequested", core.system.fatal_stop_requested, false);
        web_json_kv_str(response, "startupFailureStage", core.system.startup_failure_stage, false);
        web_json_kv_str(response, "startupRecoveryStage", core.system.startup_recovery_stage, false);
        web_json_kv_str(response, "page", core.system.current_page, false);
        web_json_kv_str(response, "timeSource", core.system.time_source, false);
        web_json_kv_str(response, "timeConfidence", core.system.time_confidence, false);
        web_json_kv_bool(response, "timeValid", core.system.time_valid, false);
        web_json_kv_bool(response, "timeAuthoritative", core.system.time_authoritative, false);
        web_json_kv_u32(response, "timeSourceAgeMs", core.system.time_source_age_ms, false);
        web_json_kv_u32(response, "uptimeMs", core.system.uptime_ms, true);
        response += "},";
        response += "\"summary\":{";
        web_json_kv_str(response, "headerTags", core.system.header_tags, false);
        web_json_kv_str(response, "networkLine", core.weather.network_line, false);
        web_json_kv_str(response, "networkSubline", core.weather.network_subline, false);
        web_json_kv_str(response, "sensorLabel", core.labels.sensor_label, false);
        web_json_kv_str(response, "storageLabel", core.labels.storage_label, false);
        web_json_kv_str(response, "diagLabel", core.labels.diag_label, false);
        web_json_kv_str(response, "networkLabel", core.weather.network_label, true);
        response += "},";
        response += "\"weather\":{";
        web_json_kv_bool(response, "valid", core.weather.valid, false);
        web_json_kv_str(response, "location", core.weather.location, false);
        web_json_kv_str(response, "text", core.weather.text, false);
        web_json_kv_bool(response, "tlsVerified", core.weather.tls_verified, false);
        web_json_kv_bool(response, "tlsCaLoaded", core.weather.tls_ca_loaded, false);
        web_json_kv_str(response, "tlsMode", core.weather.tls_mode, false);
        web_json_kv_str(response, "syncStatus", core.weather.sync_status, false);
        web_json_kv_i32(response, "lastHttpStatus", core.weather.last_http_status, false);
        web_json_kv_f32(response, "temperatureC", (float)core.weather.temperature_tenths_c / 10.0f, 1, true);
        response += "}}";
        request->send(200, "application/json", response);
    });

    server.on("/api/state/detail", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_state_detail_response(request);
    });

    server.on("/api/state/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_state_raw_response(request);
    });

    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
        WebStateCoreSnapshot core;
        WebStateDetailSnapshot detail;
        DeviceConfigSnapshot cfg;
        String response;
        response.reserve(4096);

        if (!web_state_core_collect(&core) || !web_state_detail_collect(&detail)) {
            request->send(500, "application/json", "{\"ok\":false,\"message\":\"snapshot unavailable\"}");
            return;
        }
        device_config_init();
        (void)device_config_get(&cfg);

        response = "{";
        response += "\"ok\":true,";
        web_json_kv_u32(response, "apiVersion", kWebApiVersion, false);
        web_json_kv_u32(response, "stateVersion", kWebStateVersion, false);

        response += "\"wifi\":{";
        web_json_kv_bool(response, "connected", core.wifi.connected, false);
        web_json_kv_bool(response, "provisioningApActive", core.wifi.provisioning_ap_active, false);
        web_json_kv_str(response, "provisioningApSsid", core.wifi.provisioning_ap_ssid, false);
        web_json_kv_str(response, "mode", core.wifi.mode, false);
        web_json_kv_str(response, "status", core.wifi.status, false);
        web_json_kv_str(response, "ip", core.wifi.ip, false);
        web_json_kv_i32(response, "rssi", core.wifi.rssi, true);
        response += "},";

        response += "\"system\":{";
        web_json_kv_u32(response, "uptimeMs", core.system.uptime_ms, false);
        web_json_kv_bool(response, "appReady", core.system.app_ready, false);
        web_json_kv_bool(response, "appDegraded", core.system.app_degraded, false);
        web_json_kv_bool(response, "startupOk", core.system.startup_ok, false);
        web_json_kv_bool(response, "startupDegraded", core.system.startup_degraded, false);
        web_json_kv_bool(response, "fatalStopRequested", core.system.fatal_stop_requested, false);
        web_json_kv_str(response, "startupFailureStage", core.system.startup_failure_stage, false);
        web_json_kv_str(response, "startupRecoveryStage", core.system.startup_recovery_stage, false);
        web_json_kv_str(response, "appInitStage", core.system.app_init_stage, false);
        web_json_kv_str(response, "page", core.system.current_page, false);
        web_json_kv_str(response, "timeSource", core.system.time_source, false);
        web_json_kv_str(response, "timeConfidence", core.system.time_confidence, false);
        web_json_kv_bool(response, "timeValid", core.system.time_valid, false);
        web_json_kv_bool(response, "timeAuthoritative", core.system.time_authoritative, false);
        web_json_kv_u32(response, "timeSourceAgeMs", core.system.time_source_age_ms, false);
        web_json_kv_bool(response, "sleeping", core.system.sleeping, false);
        web_json_kv_bool(response, "animating", core.system.animating, false);
        web_json_kv_bool(response, "safeMode", detail.diag.safe_mode_active, true);
        response += "},";

        response += "\"config\":{";
        web_json_kv_bool(response, "wifiConfigured", cfg.wifi_configured, false);
        web_json_kv_bool(response, "weatherConfigured", cfg.weather_configured, false);
        web_json_kv_bool(response, "authRequired", cfg.api_token_configured, false);
        web_json_kv_str(response, "wifiSsid", cfg.wifi_ssid, false);
        web_json_kv_str(response, "timezoneId", cfg.timezone_id, false);
        web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
        web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
        web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
        web_json_kv_str(response, "locationName", cfg.location_name, true);
        response += "},";

        response += "\"activity\":{";
        web_json_kv_u32(response, "steps", core.activity.steps, false);
        web_json_kv_u32(response, "goal", core.activity.goal, false);
        web_json_kv_u32(response, "goalPercent", core.activity.goal_percent, true);
        response += "},";

        response += "\"alarm\":{";
        web_json_kv_u32(response, "nextIndex", detail.alarm.next_alarm_index, false);
        web_json_kv_str(response, "nextTime", detail.alarm.next_time, false);
        web_json_kv_bool(response, "enabled", detail.alarm.enabled, false);
        web_json_kv_bool(response, "ringing", detail.alarm.ringing, false);
        web_json_kv_str(response, "label", core.labels.alarm_label, true);
        response += "},";

        response += "\"music\":{";
        web_json_kv_bool(response, "available", detail.music.available, false);
        web_json_kv_bool(response, "playing", detail.music.playing, false);
        web_json_kv_str(response, "state", detail.music.state, false);
        web_json_kv_str(response, "song", detail.music.song, false);
        web_json_kv_str(response, "label", core.labels.music_label, true);
        response += "},";

        response += "\"sensor\":{";
        web_json_kv_bool(response, "online", detail.sensor.online, false);
        web_json_kv_bool(response, "calibrated", detail.sensor.calibrated, false);
        web_json_kv_bool(response, "staticNow", detail.sensor.static_now, false);
        web_json_kv_str(response, "runtimeState", detail.sensor.runtime_state, false);
        web_json_kv_str(response, "calibrationState", detail.sensor.calibration_state, false);
        web_json_kv_u32(response, "quality", detail.sensor.quality, false);
        web_json_kv_u32(response, "errorCode", detail.sensor.error_code, false);
        web_json_kv_u32(response, "faultCount", detail.sensor.fault_count, false);
        web_json_kv_u32(response, "reinitCount", detail.sensor.reinit_count, false);
        web_json_kv_u32(response, "calibrationProgress", detail.sensor.calibration_progress, false);
        web_json_kv_i32(response, "ax", detail.sensor.ax, false);
        web_json_kv_i32(response, "ay", detail.sensor.ay, false);
        web_json_kv_i32(response, "az", detail.sensor.az, false);
        web_json_kv_i32(response, "gx", detail.sensor.gx, false);
        web_json_kv_i32(response, "gy", detail.sensor.gy, false);
        web_json_kv_i32(response, "gz", detail.sensor.gz, false);
        web_json_kv_i32(response, "accelNormMg", detail.sensor.accel_norm_mg, false);
        web_json_kv_i32(response, "baselineMg", detail.sensor.baseline_mg, false);
        web_json_kv_i32(response, "motionScore", detail.sensor.motion_score, false);
        web_json_kv_u32(response, "lastSampleMs", detail.sensor.last_sample_ms, false);
        web_json_kv_u32(response, "stepsTotal", detail.sensor.steps_total, false);
        web_json_kv_f32(response, "pitchDeg", detail.sensor.pitch_deg, 2, false);
        web_json_kv_f32(response, "rollDeg", detail.sensor.roll_deg, 2, true);
        response += "},";

        response += "\"storage\":{";
        web_json_kv_str(response, "backend", detail.storage.backend, false);
        web_json_kv_str(response, "backendPhase", detail.storage.backend_phase, false);
        web_json_kv_str(response, "commitState", detail.storage.commit_state, false);
        web_json_kv_u32(response, "schemaVersion", detail.storage.schema_version, false);
        web_json_kv_bool(response, "flashSupported", detail.storage.flash_supported, false);
        web_json_kv_bool(response, "flashReady", detail.storage.flash_ready, false);
        web_json_kv_bool(response, "migrationAttempted", detail.storage.migration_attempted, false);
        web_json_kv_bool(response, "migrationOk", detail.storage.migration_ok, false);
        web_json_kv_bool(response, "transactionActive", detail.storage.transaction_active, false);
        web_json_kv_bool(response, "sleepFlushPending", detail.storage.sleep_flush_pending, true);
        response += "},";

        response += "\"diag\":{";
        web_json_kv_bool(response, "hasLastFault", detail.diag.has_last_fault, false);
        web_json_kv_str(response, "lastFaultName", detail.diag.last_fault_name, false);
        web_json_kv_str(response, "lastFaultSeverity", detail.diag.last_fault_severity, false);
        web_json_kv_u32(response, "lastFaultCount", detail.diag.last_fault_count, false);
        web_json_kv_bool(response, "hasLastLog", detail.diag.has_last_log, false);
        web_json_kv_str(response, "lastLogName", detail.diag.last_log_name, false);
        web_json_kv_u32(response, "lastLogValue", detail.diag.last_log_value, false);
        web_json_kv_u32(response, "lastLogAux", detail.diag.last_log_aux, true);
        response += "},";

        response += "\"display\":{";
        web_json_kv_u32(response, "presentCount", detail.display.present_count, false);
        web_json_kv_u32(response, "txFailCount", detail.display.tx_fail_count, true);
        response += "},";

        response += "\"weather\":{";
        web_json_kv_bool(response, "valid", core.weather.valid, false);
        web_json_kv_str(response, "location", core.weather.location, false);
        web_json_kv_str(response, "text", core.weather.text, false);
        web_json_kv_bool(response, "tlsVerified", core.weather.tls_verified, false);
        web_json_kv_bool(response, "tlsCaLoaded", core.weather.tls_ca_loaded, false);
        web_json_kv_str(response, "tlsMode", core.weather.tls_mode, false);
        web_json_kv_str(response, "syncStatus", core.weather.sync_status, false);
        web_json_kv_i32(response, "lastHttpStatus", core.weather.last_http_status, false);
        web_json_kv_f32(response, "temperatureC", (float)core.weather.temperature_tenths_c / 10.0f, 1, false);
        web_json_kv_u32(response, "updatedAtMs", core.weather.updated_at_ms, true);
        response += "},";

        response += "\"summary\":{";
        web_json_kv_str(response, "headerTags", core.system.header_tags, false);
        web_json_kv_str(response, "networkLine", core.weather.network_line, false);
        web_json_kv_str(response, "networkSubline", core.weather.network_subline, false);
        web_json_kv_str(response, "sensorLabel", core.labels.sensor_label, false);
        web_json_kv_str(response, "storageLabel", core.labels.storage_label, false);
        web_json_kv_str(response, "diagLabel", core.labels.diag_label, true);
        response += "},";

        response += "\"terminal\":{";
        web_json_kv_str(response, "systemFace", core.system.system_face, false);
        web_json_kv_str(response, "brightnessLabel", core.system.brightness_label, false);
        web_json_kv_str(response, "activityLabel", core.activity.activity_label, false);
        web_json_kv_str(response, "sensorLabel", core.labels.sensor_label, false);
        web_json_kv_str(response, "networkLabel", core.weather.network_label, false);
        web_json_kv_str(response, "storageLabel", core.labels.storage_label, true);
        response += "},";

        response += "\"overlay\":{";
        web_json_kv_bool(response, "active", detail.overlay.active, false);
        web_json_kv_str(response, "text", detail.overlay.text, false);
        web_json_kv_u32(response, "expireAtMs", detail.overlay.expire_at_ms, true);
        response += "}";

        response += "}";
        request->send(200, "application/json", response);
    });

    server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
        const SensorSnapshot *snap = sensor_service_get_snapshot();
        String response = "{";

        if (snap == nullptr) {
            request->send(500, "application/json", "{\"ok\":false,\"message\":\"sensor unavailable\"}");
            return;
        }

        response += "\"ok\":true,";
        response += "\"online\":";
        response += snap->online ? "true" : "false";
        response += ",";
        web_json_kv_str(response, "runtimeState", sensor_runtime_state_name(snap->runtime_state), false);
        web_json_kv_str(response, "calibrationState", sensor_calibration_state_name(snap->calibration_state), false);
        web_json_kv_bool(response, "calibrated", snap->calibrated, false);
        web_json_kv_bool(response, "staticNow", snap->static_now, false);
        web_json_kv_u32(response, "quality", snap->quality, false);
        web_json_kv_u32(response, "errorCode", snap->error_code, false);
        web_json_kv_u32(response, "faultCount", snap->fault_count, false);
        web_json_kv_u32(response, "reinitCount", snap->reinit_count, false);
        web_json_kv_u32(response, "calibrationProgress", snap->calibration_progress, false);
        web_json_kv_i32(response, "ax", snap->ax, false);
        web_json_kv_i32(response, "ay", snap->ay, false);
        web_json_kv_i32(response, "az", snap->az, false);
        web_json_kv_i32(response, "gx", snap->gx, false);
        web_json_kv_i32(response, "gy", snap->gy, false);
        web_json_kv_i32(response, "gz", snap->gz, false);
        web_json_kv_i32(response, "accelNormMg", snap->accel_norm_mg, false);
        web_json_kv_i32(response, "baselineMg", snap->baseline_mg, false);
        web_json_kv_i32(response, "motionScore", snap->motion_score, false);
        web_json_kv_i32(response, "pitchDeg", snap->pitch_deg, false);
        web_json_kv_i32(response, "rollDeg", snap->roll_deg, false);
        web_json_kv_u32(response, "stepsTotal", snap->steps_total, false);
        web_json_kv_u32(response, "lastSampleMs", snap->last_sample_ms, true);
        response += "}";

        request->send(200, "application/json", response);
    });

    server.on("/api/display/frame", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response;
        response.reserve(96U + (OLED_BUFFER_SIZE * 2U));
        response += "{\"ok\":true,\"width\":";
        response += OLED_WIDTH;
        response += ",\"height\":";
        response += OLED_HEIGHT;
        response += ",\"presentCount\":";
        response += display_get_present_count();
        response += ",\"bufferHex\":\"";
        for (size_t i = 0; i < OLED_BUFFER_SIZE; ++i) {
            append_hex_byte(response, g_oled_buffer[i]);
        }
        response += "\"}";
        request->send(200, "application/json", response);
    });

    server.on("/api/input/key", HTTP_POST, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> json_doc;
                String key_str;
        String event_str;
        KeyId key_id = KEY_ID_UP;
        KeyEventType event_type = KEY_EVENT_SHORT;
        bool parsed = false;

        if (!require_control_auth(request)) {
            return;
        }

        if (read_form_field(request, "key", key_str)) {
            parsed = parse_key_id(key_str.c_str(), &key_id);
            if (read_form_field(request, "event", event_str) && event_str.length() > 0U) {
                if (!parse_key_event(event_str.c_str(), &event_type)) {
                    send_json_error(request, 400, "invalid key event");
                    return;
                }
            }
        } else if (parse_json_body(request, json_doc)) {
                        if (!read_json_string_field(json_doc, "key", key_str)) {
                send_json_error(request, 400, "missing key");
                return;
            }
            parsed = parse_key_id(key_str.c_str(), &key_id);
            if (read_json_string_field(json_doc, "event", event_str) && event_str.length() > 0U) {
                if (!parse_key_event(event_str.c_str(), &event_type)) {
                    send_json_error(request, 400, "invalid key event");
                    return;
                }
            }
        } else {
            send_json_error(request, 400, "missing key payload");
            return;
        }

        if (!parsed) {
            send_json_error(request, 400, "invalid key");
            return;
        }

        WebAction action = {};
        action.type = WEB_ACTION_KEY;
        action.data.key.id = key_id;
        action.data.key.event_type = event_type;

        uint32_t action_id = 0U;
        if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
            send_json_error(request, 503, "action queue full");
            return;
        }

        send_queue_ack(request, action_id, action.type);
    }, nullptr, capture_request_body);

    server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> json_doc;
                String cmd_str;
        AppCommandType cmd_type = (AppCommandType)0;

        if (!require_control_auth(request)) {
            return;
        }

        if (read_form_field(request, "type", cmd_str)) {
            if (!parse_command_type(cmd_str.c_str(), &cmd_type) || cmd_type == 0) {
                send_json_error(request, 400, "invalid command");
                return;
            }
        } else if (parse_json_body(request, json_doc)) {
                        if (!read_json_string_field(json_doc, "type", cmd_str)) {
                send_json_error(request, 400, "missing command type");
                return;
            }
            if (!parse_command_type(cmd_str.c_str(), &cmd_type) || cmd_type == 0) {
                send_json_error(request, 400, "invalid command");
                return;
            }
        } else {
            send_json_error(request, 400, "missing command payload");
            return;
        }

        WebAction action = {};
        action.type = WEB_ACTION_COMMAND;
        action.data.command.source = APP_COMMAND_SOURCE_UI;
        action.data.command.type = cmd_type;
        action.data.command.data.u32 = 0;

        uint32_t action_id = 0U;
        if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
            send_json_error(request, 503, "action queue full");
            return;
        }

        send_queue_ack(request, action_id, action.type);
    }, nullptr, capture_request_body);

    server.on("/api/display/overlay", HTTP_POST, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> json_doc;
        String overlay_text;
        String duration_str;
        uint32_t duration_ms = WEB_API_DEFAULT_OVERLAY_DURATION_MS;

        if (!require_control_auth(request)) {
            return;
        }

        if (read_form_field(request, "text", overlay_text)) {
            if (read_form_field(request, "durationMs", duration_str) && duration_str.length() > 0U) {
                if (!parse_strict_u32_field(duration_str, &duration_ms)) {
                    send_json_error(request, 400, "invalid overlay duration");
                    return;
                }
            }
        } else if (parse_json_body(request, json_doc)) {
            if (!read_json_string_field(json_doc, "text", overlay_text)) {
                send_json_error(request, 400, "missing overlay text");
                return;
            }
            if (!json_doc["durationMs"].isNull() && !read_json_u32_field(json_doc, "durationMs", &duration_ms)) {
                send_json_error(request, 400, "invalid overlay duration");
                return;
            }
        } else {
            send_json_error(request, 400, "missing overlay payload");
            return;
        }

        if (overlay_text.length() == 0U ||
            overlay_text.length() > WEB_API_OVERLAY_MAX_TEXT_LEN ||
            duration_ms == 0U ||
            duration_ms > WEB_API_MAX_OVERLAY_DURATION_MS) {
            send_json_error(request, 400, "invalid overlay params");
            return;
        }

        WebAction action = {};
        action.type = WEB_ACTION_OVERLAY_TEXT;
        strncpy(action.data.overlay.text, overlay_text.c_str(), sizeof(action.data.overlay.text) - 1U);
        action.data.overlay.text[sizeof(action.data.overlay.text) - 1U] = '\0';
        action.data.overlay.duration_ms = duration_ms;

        uint32_t action_id = 0U;
        if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
            send_json_error(request, 503, "action queue full");
            return;
        }

        send_queue_ack(request, action_id, action.type);
    }, nullptr, capture_request_body);

    server.on("/api/display/overlay/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        WebAction action = {};
        if (!require_control_auth(request)) {
            return;
        }
        action.type = WEB_ACTION_OVERLAY_CLEAR;
        uint32_t action_id = 0U;
        if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
            send_json_error(request, 503, "action queue full");
            return;
        }
        send_queue_ack(request, action_id, action.type);
    }, nullptr, capture_request_body);
}
