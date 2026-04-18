#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "web/web_action_queue.h"
#include "web/web_state_bridge.h"
#include "web/web_state_bridge_internal.h"
#include "web/web_json.h"
#include "web/web_wifi.h"
#include "web/web_server.h"
#include "web/web_api_config.h"
#include "web/web_runtime_snapshot.h"
#include "web/web_contract.h"
#include "web/web_contract_json.h"
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>
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
#include "board_manifest.h"
#include "system_init.h"
#include "services/runtime_event_service.h"
#include "services/runtime_reload_coordinator.h"
#include "services/reset_service.h"
#include "services/storage_semantics.h"
#include "services/storage_facade.h"
#include "services/device_config_authority.h"
}

static const char *kHexDigits = "0123456789ABCDEF";

static void append_reset_action_section(String &response, const ResetActionReport &report, bool append_trailing);
static void append_reset_action_schema_sections(String &response, const ResetActionReport &report);
static void append_health_schema_sections(String &response);
static void append_display_frame_schema_sections(String &response);
static void append_tracked_action_ack_schema_sections(String &response, uint32_t action_id, WebActionType type);


static bool web_command_supported(const AppCommandDescriptor *descriptor)
{
    if (descriptor == nullptr) {
        return false;
    }
    switch (descriptor->type) {
        case APP_COMMAND_SET_VIBRATE:
#if APP_FEATURE_VIBRATION
            return true;
#else
            return false;
#endif
        default:
            return true;
    }
}

static const char *feature_lifecycle_name(bool enabled)
{
    return enabled ? "ACTIVE" : "ARCHIVED";
}
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

static void append_runtime_reload_domain_array(String &response,
                                               const char *key,
                                               uint32_t domain_mask,
                                               bool last)
{
    response += '"';
    response += key;
    response += "\":";
    response += '[';
    bool appended = false;
    if ((domain_mask & RUNTIME_RELOAD_DOMAIN_WIFI) != 0U) {
        response += '"';
        response += "WIFI";
        response += '"';
        appended = true;
    }
    if ((domain_mask & RUNTIME_RELOAD_DOMAIN_NETWORK) != 0U) {
        if (appended) {
            response += ',';
        }
        response += '"';
        response += "NETWORK";
        response += '"';
    }
    response += ']';
    if (!last) {
        response += ',';
    }
}

static void append_runtime_reload_payload(String &response, const RuntimeReloadReport &reload, bool append_trailing)
{
    const bool runtime_reloaded = (!reload.requested) || (reload.wifi_reload_ok && reload.network_reload_ok && reload.verify_ok);

    web_json_kv_bool(response, "runtimeReloadRequested", reload.requested, false);
    web_json_kv_bool(response, "runtimeReloadPreflightOk", reload.preflight_ok, false);
    web_json_kv_bool(response, "runtimeReloadApplyAttempted", reload.apply_attempted, false);
    web_json_kv_bool(response, "runtimeReloaded", runtime_reloaded, false);
    web_json_kv_bool(response, "runtimeReloadEventDispatchOk", reload.event_dispatch_ok, false);
    web_json_kv_bool(response, "runtimeReloadAuthoritativePath", true, false);
    web_json_kv_bool(response, "runtimeReloadVerifyOk", reload.verify_ok, false);
    web_json_kv_bool(response, "runtimeReloadPartialSuccess", reload.partial_success, false);
    web_json_kv_bool(response, "runtimeWifiReloadOk", reload.wifi_reload_ok, false);
    web_json_kv_bool(response, "runtimeNetworkReloadOk", reload.network_reload_ok, false);
    web_json_kv_u32(response, "runtimeHandlerCount", reload.handler_count, false);
    web_json_kv_u32(response, "runtimeHandlerSuccessCount", reload.handler_success_count, false);
    web_json_kv_u32(response, "runtimeHandlerFailureCount", reload.handler_failure_count, false);
    web_json_kv_u32(response, "runtimeHandlerCriticalFailureCount", reload.handler_critical_failure_count, false);
    web_json_kv_u32(response, "runtimeReloadConfigGeneration", reload.config_generation, false);
    web_json_kv_u32(response, "runtimeWifiAppliedGeneration", reload.wifi_applied_generation, false);
    web_json_kv_u32(response, "runtimeNetworkAppliedGeneration", reload.network_applied_generation, false);
    web_json_kv_str(response, "runtimeWifiVerifyReason", reload.wifi_verify_reason != NULL ? reload.wifi_verify_reason : "NONE", false);
    web_json_kv_str(response, "runtimeNetworkVerifyReason", reload.network_verify_reason != NULL ? reload.network_verify_reason : "NONE", false);
    append_runtime_reload_domain_array(response, "runtimeReloadImpactDomains", reload.requested_domain_mask, false);
    append_runtime_reload_domain_array(response, "runtimeReloadAppliedDomains", reload.applied_domain_mask, false);
    append_runtime_reload_domain_array(response, "runtimeReloadFailedDomains", reload.failed_domain_mask, false);
    web_json_kv_str(response, "runtimeReloadFailurePhase", reload.failure_phase != NULL ? reload.failure_phase : "NONE", false);
    web_json_kv_str(response, "runtimeReloadFailureCode", reload.failure_code != NULL ? reload.failure_code : "NONE", append_trailing);
}

static void append_reset_report_payload(String &response, const ResetActionReport &report, bool append_trailing)
{
    web_json_kv_str(response,
                    "resetKind",
                    report.kind == RESET_ACTION_FACTORY ? "FACTORY" :
                    (report.kind == RESET_ACTION_DEVICE_CONFIG ? "DEVICE_CONFIG" : "APP_STATE"),
                    false);
    web_json_kv_bool(response, "appStateReset", report.app_state_reset, false);
    web_json_kv_bool(response, "deviceConfigReset", report.device_config_reset, false);
    web_json_kv_bool(response, "auditNotified", report.audit_notified, false);
    web_json_kv_bool(response, "auditEventDispatched", report.audit_event_dispatched, false);
    append_runtime_reload_payload(response, report.reload, append_trailing);
}

static void send_reset_action_response(AsyncWebServerRequest *request,
                                       bool ok,
                                       const ResetActionReport &report,
                                       const char *success_message,
                                       const char *partial_failure_message,
                                       const char *hard_failure_message)
{
    bool requested_reload = report.runtime_reload_requested || report.reload.requested;
    bool hard_failure = false;
    int status_code = 200;
    const char *message = success_message;
    String response = "{";

    switch (report.kind) {
        case RESET_ACTION_APP_STATE:
            hard_failure = !report.app_state_reset;
            break;
        case RESET_ACTION_DEVICE_CONFIG:
            hard_failure = !report.device_config_reset;
            break;
        case RESET_ACTION_FACTORY:
            hard_failure = !report.app_state_reset || !report.device_config_reset;
            break;
        default:
            hard_failure = !ok;
            break;
    }

    if (!ok) {
        if (!hard_failure && requested_reload && !report.runtime_reload_ok) {
            status_code = 202;
            message = partial_failure_message;
        } else {
            status_code = 500;
            message = hard_failure_message;
        }
    }

    response += "\"ok\":";
    response += ok ? "true," : "false,";
    web_json_kv_str(response, "message", message, false);
    response += "\"runtimeReload\":{";
    append_runtime_reload_payload(response, report.reload, true);
    response += "},";
    append_reset_report_payload(response, report, false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
    web_json_kv_bool(response, "assetContractReady", web_server_asset_contract_ready(), false);
    web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
    web_json_kv_bool(response, "hardFailure", hard_failure, false);
    web_json_kv_bool(response, "completedWithReloadFailure", !hard_failure && requested_reload && !report.runtime_reload_ok, false);
    append_reset_action_schema_sections(response, report);
    response += ',';
    web_json_kv_str(response, "messageCompat", message, true);
    response += "}";
    request->send(status_code, "application/json", response);
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
    auto field = root[name];
    if (field.isNull() || !field.template is<const char *>()) {
        return false;
    }
    value = field.template as<const char *>();
    return true;
}

template <typename TRoot>
static bool read_json_u32_field(const TRoot &root, const char *name, uint32_t *value)
{
    auto field = root[name];
    if (value == nullptr || field.isNull()) {
        return false;
    }
    if (field.template is<int>()) {
        int signed_value = field.template as<int>();
        if (signed_value < 0) {
            return false;
        }
        *value = (uint32_t)signed_value;
        return true;
    }
    if (field.template is<unsigned int>()) {
        *value = (uint32_t)field.template as<unsigned int>();
        return true;
    }
    if (field.template is<uint32_t>()) {
        *value = field.template as<uint32_t>();
        return true;
    }
    if (field.template is<unsigned long>()) {
        unsigned long raw = field.template as<unsigned long>();
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
    auto field = root[name];
    if (value == nullptr || field.isNull()) {
        return false;
    }
    if (!field.template is<float>() && !field.template is<double>() && !field.template is<int>()) {
        return false;
    }
    *value = field.template as<float>();
    return std::isfinite((double)(*value));
}

template <typename TRoot>
static bool read_json_bool_field(const TRoot &root, const char *name, bool *value)
{
    auto field = root[name];
    if (value == nullptr || field.isNull() || !field.template is<bool>()) {
        return false;
    }
    *value = field.template as<bool>();
    return true;
}

static bool populate_command_payload_from_request(AsyncWebServerRequest *request,
                                                  const StaticJsonDocument<256> *json_doc,
                                                  AppCommand *command,
                                                  const String &cmd_str,
                                                  String *out_error)
{
    String value_str;
    uint32_t u32_value = 0U;
    const char *integer_error = "command requires integer field `value`";
    const char *bool_error = "command requires boolean field `enabled`";

    if (command == nullptr) {
        if (out_error != nullptr) {
            *out_error = "invalid command payload";
        }
        return false;
    }

    switch (command->type) {
        case APP_COMMAND_SET_BRIGHTNESS:
        case APP_COMMAND_SET_WATCHFACE:
            if (json_doc != nullptr) {
                if (!read_json_u32_field(*json_doc, "value", &u32_value)) {
                    if (out_error != nullptr) {
                        *out_error = integer_error;
                    }
                    return false;
                }
            } else {
                if (!read_form_field(request, "value", value_str) || !parse_strict_u32_field(value_str, &u32_value)) {
                    if (out_error != nullptr) {
                        *out_error = integer_error;
                    }
                    return false;
                }
            }
            command->data.u8 = (uint8_t)(u32_value & 0xFFU);
            return true;
        case APP_COMMAND_SET_SHOW_SECONDS: {
            bool enabled = false;
            if (json_doc != nullptr) {
                if (!read_json_bool_field(*json_doc, "enabled", &enabled)) {
                    if (out_error != nullptr) {
                        *out_error = bool_error;
                    }
                    return false;
                }
            } else {
                const char *raw = nullptr;
                if (!read_form_field(request, "enabled", value_str)) {
                    if (out_error != nullptr) {
                        *out_error = bool_error;
                    }
                    return false;
                }
                raw = value_str.c_str();
                if (strcmp(raw, "1") == 0 || strcmp(raw, "true") == 0 || strcmp(raw, "on") == 0) {
                    enabled = true;
                } else if (strcmp(raw, "0") == 0 || strcmp(raw, "false") == 0 || strcmp(raw, "off") == 0) {
                    enabled = false;
                } else {
                    if (out_error != nullptr) {
                        *out_error = bool_error;
                    }
                    return false;
                }
            }
            command->data.enabled = enabled;
            return true;
        }
        default:
            (void)cmd_str;
            return true;
    }
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
    if (!device_config_authority_has_token()) {
        return true;
    }
    if (!auth_token_from_request(request, token)) {
        return false;
    }
    return device_config_authority_authenticate_token(token.c_str());
}

static bool control_is_locked_in_provisioning_ap(void)
{
    return !device_config_authority_has_token() && web_wifi_access_point_active() && !web_wifi_connected();
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
    web_json_kv_u32(response, "id", status.id, false);
    web_json_kv_u32(response, "actionId", status.id, false);
    web_json_kv_u32(response, "requestId", status.id, false);
    web_json_kv_str(response, "type", web_action_type_name(status.type), false);
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
    response += "\"trackPath\":\"";
    response += WEB_ROUTE_ACTIONS_STATUS;
    response += "?id=";
    response += action_id;
    response += "\",";
    web_json_kv_u32(response, "queueDepth", web_action_queue_depth(), false);
    append_tracked_action_ack_schema_sections(response, action_id, type);
    response += "}";
    request->send(200, "application/json", response);
}

static void append_command_catalog(String &response)
{
    size_t i;
    response += "\"commandCatalogVersion\":";
    response += WEB_COMMAND_CATALOG_VERSION;
    response += ",\"commands\":[";
    for (i = 0; i < app_command_catalog_count(); ++i) {
        const AppCommandDescriptor *descriptor = app_command_catalog_at(i);
        if (descriptor == nullptr || !descriptor->web_exposed || !web_command_supported(descriptor)) {
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

static void append_command_catalog_section(String &response, bool append_trailing)
{
    response += "\"commandCatalog\":{";
    append_command_catalog(response);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_action_status_section(String &response, const WebActionStatusSnapshot &status, bool append_trailing)
{
    response += "\"actionStatus\":{";
    web_json_kv_u32(response, "id", status.id, false);
    web_json_kv_str(response, "type", web_action_type_name(status.type), false);
    web_json_kv_str(response, "status", web_action_status_name(status.status), false);
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
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_runtime_reload_section(String &response, const RuntimeReloadReport &reload, bool append_trailing)
{
    response += "\"runtimeReload\":{";
    append_runtime_reload_payload(response, reload, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_reset_action_section(String &response, const ResetActionReport &report, bool append_trailing)
{
    response += "\"resetAction\":{";
    web_json_kv_str(response,
                    "resetKind",
                    report.kind == RESET_ACTION_FACTORY ? "FACTORY" :
                    (report.kind == RESET_ACTION_DEVICE_CONFIG ? "DEVICE_CONFIG" : "APP_STATE"),
                    false);
    web_json_kv_bool(response, "appStateReset", report.app_state_reset, false);
    web_json_kv_bool(response, "deviceConfigReset", report.device_config_reset, false);
    web_json_kv_bool(response, "auditNotified", report.audit_notified, false);
    web_json_kv_bool(response, "auditEventDispatched", report.audit_event_dispatched, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_device_config_update_section(String &response,
                                                bool wifi_saved,
                                                bool network_saved,
                                                bool token_saved,
                                                bool filesystem_ready,
                                                bool filesystem_assets_ready,
                                                const char *filesystem_status,
                                                bool append_trailing)
{
    response += "\"deviceConfigUpdate\":{";
    web_json_kv_bool(response, "wifiSaved", wifi_saved, false);
    web_json_kv_bool(response, "networkSaved", network_saved, false);
    web_json_kv_bool(response, "tokenSaved", token_saved, false);
    web_json_kv_bool(response, "filesystemReady", filesystem_ready, false);
    web_json_kv_bool(response, "filesystemAssetsReady", filesystem_assets_ready, false);
    web_json_kv_str(response, "filesystemStatus", filesystem_status, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
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
    web_json_kv_bool(response, "statePerf", true, false);
    web_json_kv_bool(response, "stateRaw", true, false);
    web_json_kv_bool(response, "controlLockInProvisioningAp", true, true);
    response += "}";
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
    WEB_STATE_PAYLOAD_SUMMARY = 0,
    WEB_STATE_PAYLOAD_DETAIL,
    WEB_STATE_PAYLOAD_PERF,
    WEB_STATE_PAYLOAD_RAW,
    WEB_STATE_PAYLOAD_AGGREGATE,
};

static const WebStateSectionSpec g_web_state_view_summary[] = {
    {WEB_STATE_SECTION_WIFI, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_SYSTEM, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_SUMMARY, WEB_STATE_SECTION_FLAG_SUMMARY_INCLUDE_NETWORK_LABEL},
    {WEB_STATE_SECTION_WEATHER, WEB_STATE_SECTION_FLAG_NONE},
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
    {WEB_STATE_SECTION_SENSOR, WEB_STATE_SECTION_FLAG_SENSOR_INCLUDE_RAW_AXES},
    {WEB_STATE_SECTION_STORAGE, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_DIAG, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_DISPLAY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_WEATHER, WEB_STATE_SECTION_FLAG_WEATHER_INCLUDE_UPDATED_AT},
    {WEB_STATE_SECTION_SUMMARY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_TERMINAL, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_OVERLAY, WEB_STATE_SECTION_FLAG_NONE},
    {WEB_STATE_SECTION_PERF, WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY},
};

static const WebStateSectionSpec *web_state_view_schema(WebStatePayloadView view, size_t *out_count)
{
    const WebStateSectionSpec *schema = g_web_state_view_aggregate;
    size_t count = sizeof(g_web_state_view_aggregate) / sizeof(g_web_state_view_aggregate[0]);

    switch (view) {
        case WEB_STATE_PAYLOAD_SUMMARY:
            schema = g_web_state_view_summary;
            count = sizeof(g_web_state_view_summary) / sizeof(g_web_state_view_summary[0]);
            break;
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
    web_json_kv_bool(response, "timeValid", core.system.time_valid, false);
    web_json_kv_bool(response, "timeAuthoritative", core.system.time_authoritative, false);
    web_json_kv_u32(response, "timeSourceAgeMs", core.system.time_source_age_ms, include_sleep_state || include_safe_mode);
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

    response = "{";
    response += "\"ok\":true,";
    append_state_versions(response);

    schema = web_state_view_schema(view, &section_count);
    for (size_t i = 0U; i < section_count; ++i) {
        append_state_section(response, bundle, schema[i], i + 1U == section_count);
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

static void send_contract_response(AsyncWebServerRequest *request)
{
    String response = "{";
    response += "\"ok\":true,";
    web_contract_append_json(response);
    response += "}";
    request->send(200, "application/json", response);
}

static void append_storage_semantics_array(String &response)
{
    response += "[";
    for (uint32_t i = 0U; i < storage_semantics_count(); ++i) {
        StorageObjectSemantic semantic = {};
        if (!storage_semantics_at(i, &semantic)) {
            continue;
        }
        if (response[response.length() - 1U] != '[') {
            response += ',';
        }
        response += '{';
        web_json_kv_u32(response, "kind", (uint32_t)semantic.kind, false);
        web_json_kv_str(response, "name", semantic.name, false);
        web_json_kv_str(response, "owner", semantic.owner, false);
        web_json_kv_str(response, "namespace", semantic.namespace_name, false);
        web_json_kv_str(response, "resetBehavior", storage_reset_behavior_name(semantic.reset_behavior), false);
        web_json_kv_str(response, "durability", storage_durability_level_name(semantic.durability), false);
        web_json_kv_bool(response, "managedByStorageService", semantic.managed_by_storage_service, false);
        web_json_kv_bool(response, "survivesPowerLoss", semantic.survives_power_loss, true);
        response += '}';
    }
    response += "]";
}

static void send_storage_semantics_response(AsyncWebServerRequest *request)
{
    String response = "{";
    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    response += "\"objects\":";
    append_storage_semantics_array(response);
    response += ",\"storageSemantics\":";
    append_storage_semantics_array(response);
    response += "}";
    request->send(200, "application/json", response);
}


static void append_meta_versions_section(String &response, bool append_trailing)
{
    response += "\"versions\":{";
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    web_json_kv_u32(response, "stateVersion", WEB_STATE_VERSION, false);
    web_json_kv_u32(response, "storageSchemaVersion", APP_STORAGE_VERSION, false);
    web_json_kv_u32(response, "runtimeContractVersion", WEB_RUNTIME_CONTRACT_VERSION, false);
    web_json_kv_u32(response, "commandCatalogVersion", WEB_COMMAND_CATALOG_VERSION, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_meta_storage_runtime_section(String &response, const StorageFacadeRuntimeSnapshot &storage_runtime, bool append_trailing)
{
    response += "\"storageRuntime\":{";
    web_json_kv_bool(response, "flashStorageSupported", storage_runtime.flash_supported, false);
    web_json_kv_bool(response, "flashStorageReady", storage_runtime.flash_ready, false);
    web_json_kv_bool(response, "storageMigrationAttempted", storage_runtime.migration_attempted, false);
    web_json_kv_bool(response, "storageMigrationOk", storage_runtime.migration_ok, false);
    web_json_kv_str(response, "appStateBackend", storage_runtime.app_state_backend, false);
    web_json_kv_str(response, "deviceConfigBackend", storage_runtime.device_config_backend, false);
    web_json_kv_str(response, "appStateDurability", storage_runtime.app_state_durability, false);
    web_json_kv_str(response, "deviceConfigDurability", storage_runtime.device_config_durability, false);
    web_json_kv_bool(response, "appStatePowerLossGuaranteed", storage_runtime.app_state_power_loss_guaranteed, false);
    web_json_kv_bool(response, "deviceConfigPowerLossGuaranteed", storage_runtime.device_config_power_loss_guaranteed, false);
    web_json_kv_bool(response, "appStateMixedDurability", storage_runtime.app_state_mixed_durability, false);
    web_json_kv_u32(response, "appStateResetDomainObjectCount", storage_runtime.app_state_reset_domain_object_count, false);
    web_json_kv_u32(response, "appStateDurableObjectCount", storage_runtime.app_state_durable_object_count, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_meta_asset_contract_section(String &response, bool append_trailing)
{
    response += "\"assetContract\":{";
    web_json_kv_bool(response, "webControlPlaneReady", web_server_control_plane_ready(), false);
    web_json_kv_bool(response, "webConsoleReady", web_server_console_ready(), false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
    web_json_kv_bool(response, "assetContractReady", web_server_asset_contract_ready(), false);
    web_json_kv_bool(response, "assetContractHashVerified", web_server_asset_contract_hash_verified(), false);
    web_json_kv_u32(response, "assetContractVersion", web_server_asset_contract_version(), false);
    web_json_kv_u32(response, "assetContractHash", web_server_asset_contract_hash(), false);
    web_json_kv_str(response, "assetContractGeneratedAt", web_server_asset_contract_generated_at(), false);
    web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
    web_json_kv_str(response, "assetExpectedHashIndexHtml", web_server_asset_expected_hash("index.html"), false);
    web_json_kv_str(response, "assetActualHashIndexHtml", web_server_asset_actual_hash("index.html"), false);
    web_json_kv_str(response, "assetExpectedHashAppJs", web_server_asset_expected_hash("app.js"), false);
    web_json_kv_str(response, "assetActualHashAppJs", web_server_asset_actual_hash("app.js"), false);
    web_json_kv_str(response, "assetExpectedHashAppCss", web_server_asset_expected_hash("app.css"), false);
    web_json_kv_str(response, "assetActualHashAppCss", web_server_asset_actual_hash("app.css"), false);
    web_json_kv_str(response, "assetExpectedHashContractBootstrap", web_server_asset_expected_hash("contract-bootstrap.json"), false);
    web_json_kv_str(response, "assetActualHashContractBootstrap", web_server_asset_actual_hash("contract-bootstrap.json"), true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_meta_runtime_events_section(String &response, bool append_trailing)
{
    response += "\"runtimeEvents\":{";
    web_json_kv_u32(response, "runtimeEventHandlers", runtime_event_service_handler_count(), false);
    web_json_kv_u32(response, "runtimeEventCapacity", runtime_event_service_capacity(), false);
    web_json_kv_u32(response, "runtimeEventRegistrationRejectCount", runtime_event_service_registration_reject_count(), false);
    web_json_kv_u32(response, "runtimeEventPublishCount", runtime_event_service_publish_count(), false);
    web_json_kv_u32(response, "runtimeEventPublishFailCount", runtime_event_service_publish_fail_count(), false);
    web_json_kv_u32(response, "runtimeEventLastSuccessCount", runtime_event_service_last_success_count(), false);
    web_json_kv_u32(response, "runtimeEventLastFailureCount", runtime_event_service_last_failure_count(), false);
    web_json_kv_u32(response, "runtimeEventLastCriticalFailureCount", runtime_event_service_last_critical_failure_count(), false);
    web_json_kv_str(response, "runtimeEventLast", runtime_event_service_event_name(runtime_event_service_last_event()), false);
    web_json_kv_str(response, "runtimeEventLastFailed", runtime_event_service_event_name(runtime_event_service_last_failed_event()), false);
    web_json_kv_i32(response, "runtimeEventLastFailedHandlerIndex", runtime_event_service_last_failed_handler_index(), false);
    response += "\"handlers\":";
    response += '[';
    for (uint8_t i = 0U; i < runtime_event_service_handler_count(); ++i) {
        if (i != 0U) {
            response += ',';
        }
        response += '{';
        web_json_kv_u32(response, "index", i, false);
        web_json_kv_str(response, "name", runtime_event_service_handler_name(i), false);
        web_json_kv_i32(response, "priority", runtime_event_service_handler_priority(i), false);
        web_json_kv_bool(response, "critical", runtime_event_service_handler_is_critical(i), true);
        response += '}';
    }
    response += ']';
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

struct ApiSchemaEmitContext {
    const StorageFacadeRuntimeSnapshot *storage_runtime = nullptr;
    const WebActionStatusSnapshot *action_status = nullptr;
    const ResetActionReport *reset_report = nullptr;
    const RuntimeReloadReport *runtime_reload = nullptr;
    const DeviceConfigSnapshot *device_config = nullptr;
    bool wifi_saved = false;
    bool network_saved = false;
    bool token_saved = false;
    bool filesystem_ready = false;
    bool filesystem_assets_ready = false;
    const char *filesystem_status = nullptr;
    uint32_t tracked_action_id = 0U;
    WebActionType tracked_action_type = WEB_ACTION_COMMAND;
};

static void append_meta_platform_support_section(String &response, bool append_trailing)
{
    PlatformSupportSnapshot platform_support = {};
    response += "\"platformSupport\":{";
    if (platform_get_support_snapshot(&platform_support)) {
        web_json_kv_bool(response, "rtcResetDomain", platform_support.rtc_reset_domain_supported, false);
        web_json_kv_bool(response, "idleLightSleep", platform_support.idle_light_sleep_supported, false);
        web_json_kv_bool(response, "watchdog", platform_support.watchdog_supported, false);
        web_json_kv_bool(response, "flashJournal", platform_support.flash_journal_supported, true);
    } else {
        web_json_kv_bool(response, "rtcResetDomain", false, false);
        web_json_kv_bool(response, "idleLightSleep", false, false);
        web_json_kv_bool(response, "watchdog", false, false);
        web_json_kv_bool(response, "flashJournal", false, true);
    }
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_meta_capabilities_section(String &response, bool append_trailing)
{
    append_capabilities(response);
    if (!append_trailing) {
        response += ',';
    }
}

static void append_meta_device_identity_section(String &response, bool append_trailing)
{
    char mac_buffer[18] = {0};
    const uint64_t efuse_mac = ESP.getEfuseMac();
    std::snprintf(mac_buffer, sizeof(mac_buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                  (unsigned)((efuse_mac >> 40) & 0xFFU),
                  (unsigned)((efuse_mac >> 32) & 0xFFU),
                  (unsigned)((efuse_mac >> 24) & 0xFFU),
                  (unsigned)((efuse_mac >> 16) & 0xFFU),
                  (unsigned)((efuse_mac >> 8) & 0xFFU),
                  (unsigned)(efuse_mac & 0xFFU));
    response += "\"deviceIdentity\":{";
    web_json_kv_str(response, "boardProfile", board_manifest_profile_name(), false);
    web_json_kv_str(response, "chipModel", "ESP32-S3", false);
    web_json_kv_str(response, "efuseMac", mac_buffer, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_meta_release_validation_section(String &response, bool append_trailing)
{
    response += "\"releaseValidation\":{";
    web_json_kv_u32(response, "schemaVersion", WEB_RELEASE_VALIDATION_SCHEMA_VERSION, false);
    web_json_kv_str(response, "candidateBundleKind", "candidate", false);
    web_json_kv_str(response, "verifiedBundleKind", "verified", false);
    web_json_kv_str(response, "hostReportType", "HOST_VALIDATION_REPORT", false);
    web_json_kv_str(response, "deviceReportType", "DEVICE_SMOKE_REPORT", true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_contract_document_section(String &response, bool append_trailing)
{
    web_contract_append_json(response);
    if (!append_trailing) {
        response += ',';
    }
}

static void append_storage_semantics_section(String &response, bool append_trailing)
{
    response += "\"storageSemantics\":";
    append_storage_semantics_array(response);
    if (!append_trailing) {
        response += ',';
    }
}

static void append_health_status_section(String &response, bool append_trailing)
{
    response += "\"healthStatus\":{";
    web_json_kv_bool(response, "wifiConnected", web_wifi_connected(), false);
    web_json_kv_str(response, "ip", web_wifi_ip_string(), false);
    web_json_kv_u32(response, "uptimeMs", (uint32_t)millis(), false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "assetContractReady", web_server_asset_contract_ready() && web_server_asset_contract_hash_verified(), true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_display_frame_section(String &response, bool append_trailing)
{
    response += "\"displayFrame\":{";
    web_json_kv_u32(response, "width", OLED_WIDTH, false);
    web_json_kv_u32(response, "height", OLED_HEIGHT, false);
    web_json_kv_u32(response, "presentCount", display_get_present_count(), false);
    response += "\"bufferHex\":\"";
    for (size_t i = 0; i < OLED_BUFFER_SIZE; ++i) {
        append_hex_byte(response, g_oled_buffer[i]);
    }
    response += "\"}";
    if (!append_trailing) {
        response += ',';
    }
}

static void append_tracked_action_accepted_section(String &response,
                                                   uint32_t action_id,
                                                   WebActionType type,
                                                   bool append_trailing)
{
    response += "\"trackedActionAccepted\":{";
    web_json_kv_u32(response, "actionId", action_id, false);
    web_json_kv_u32(response, "requestId", action_id, false);
    web_json_kv_str(response, "actionType", web_action_type_name(type), false);
    response += "\"trackPath\":\"";
    response += WEB_ROUTE_ACTIONS_STATUS;
    response += "?id=";
    response += action_id;
    response += "\",";
    web_json_kv_u32(response, "queueDepth", web_action_queue_depth(), true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static void append_config_device_readback_section(String &response,
                                                  const DeviceConfigSnapshot &cfg,
                                                  const StorageFacadeRuntimeSnapshot &storage_runtime,
                                                  bool append_trailing)
{
    response += "\"deviceConfigReadback\":{";
    web_json_kv_bool(response, "wifiConfigured", cfg.wifi_configured, false);
    web_json_kv_u32(response, "generation", device_config_generation(), false);
    web_json_kv_bool(response, "weatherConfigured", cfg.weather_configured, false);
    web_json_kv_bool(response, "apiTokenConfigured", cfg.api_token_configured, false);
    web_json_kv_bool(response, "authRequired", cfg.api_token_configured, false);
    web_json_kv_str(response, "wifiSsid", cfg.wifi_ssid, false);
    web_json_kv_str(response, "timezonePosix", cfg.timezone_posix, false);
    web_json_kv_str(response, "timezoneId", cfg.timezone_id, false);
    web_json_kv_f32(response, "latitude", cfg.latitude, 6, false);
    web_json_kv_f32(response, "longitude", cfg.longitude, 6, false);
    web_json_kv_str(response, "locationName", cfg.location_name, false);
    web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
    web_json_kv_str(response, "appStateDurability", storage_runtime.app_state_durability, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

static bool append_api_schema_section(String &response,
                                      const char *section,
                                      const ApiSchemaEmitContext &ctx,
                                      bool append_trailing)
{
    if (strcmp(section, "contract") == 0) {
        append_contract_document_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "storageSemantics") == 0) {
        append_storage_semantics_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "healthStatus") == 0) {
        append_health_status_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "versions") == 0 && ctx.storage_runtime != nullptr) {
        append_meta_versions_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "storageRuntime") == 0 && ctx.storage_runtime != nullptr) {
        append_meta_storage_runtime_section(response, *ctx.storage_runtime, append_trailing);
        return true;
    }
    if (strcmp(section, "assetContract") == 0) {
        append_meta_asset_contract_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "runtimeEvents") == 0) {
        append_meta_runtime_events_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "platformSupport") == 0) {
        append_meta_platform_support_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "deviceIdentity") == 0) {
        append_meta_device_identity_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "capabilities") == 0) {
        append_meta_capabilities_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "releaseValidation") == 0) {
        append_meta_release_validation_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "displayFrame") == 0) {
        append_display_frame_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "trackedActionAccepted") == 0) {
        append_tracked_action_accepted_section(response, ctx.tracked_action_id, ctx.tracked_action_type, append_trailing);
        return true;
    }
    if (strcmp(section, "commandCatalog") == 0) {
        append_command_catalog_section(response, append_trailing);
        return true;
    }
    if (strcmp(section, "actionStatus") == 0 && ctx.action_status != nullptr) {
        append_action_status_section(response, *ctx.action_status, append_trailing);
        return true;
    }
    if (strcmp(section, "resetAction") == 0 && ctx.reset_report != nullptr) {
        append_reset_action_section(response, *ctx.reset_report, append_trailing);
        return true;
    }
    if (strcmp(section, "runtimeReload") == 0 && ctx.runtime_reload != nullptr) {
        append_runtime_reload_section(response, *ctx.runtime_reload, append_trailing);
        return true;
    }
    if (strcmp(section, "deviceConfigUpdate") == 0) {
        append_device_config_update_section(response,
                                            ctx.wifi_saved,
                                            ctx.network_saved,
                                            ctx.token_saved,
                                            ctx.filesystem_ready,
                                            ctx.filesystem_assets_ready,
                                            ctx.filesystem_status,
                                            append_trailing);
        return true;
    }
    if (strcmp(section, "deviceConfigReadback") == 0 && ctx.device_config != nullptr && ctx.storage_runtime != nullptr) {
        append_config_device_readback_section(response, *ctx.device_config, *ctx.storage_runtime, append_trailing);
        return true;
    }
    return false;
}

static bool append_api_schema_sections(String &response,
                                       const char *const *sections,
                                       size_t section_count,
                                       const ApiSchemaEmitContext &ctx)
{
    for (size_t i = 0U; i < section_count; ++i) {
        if (!append_api_schema_section(response, sections[i], ctx, i + 1U == section_count)) {
            return false;
        }
    }
    return true;
}

static bool append_api_schema_sections_for_route(String &response,
                                                 const char *route_key,
                                                 const ApiSchemaEmitContext &ctx)
{
    WebApiSchemaView schema = {};
    if (!web_contract_get_route_api_schema(route_key, &schema)) {
        return false;
    }
    return append_api_schema_sections(response, schema.sections, schema.section_count, ctx);
}

static void append_meta_schema_sections(String &response, const StorageFacadeRuntimeSnapshot &storage_runtime)
{
    ApiSchemaEmitContext ctx = {};
    ctx.storage_runtime = &storage_runtime;
    (void)append_api_schema_sections_for_route(response, "meta", ctx);
}

static void append_actions_catalog_schema_sections(String &response)
{
    ApiSchemaEmitContext ctx = {};
    (void)append_api_schema_sections_for_route(response, "actionsCatalog", ctx);
}

static void append_actions_status_schema_sections(String &response, const WebActionStatusSnapshot &result)
{
    ApiSchemaEmitContext ctx = {};
    ctx.action_status = &result;
    (void)append_api_schema_sections_for_route(response, "actionsStatus", ctx);
}

static void append_reset_action_schema_sections(String &response, const ResetActionReport &report)
{
    ApiSchemaEmitContext ctx = {};
    ctx.reset_report = &report;
    ctx.runtime_reload = &report.reload;
    (void)append_api_schema_sections_for_route(response, "resetDeviceConfig", ctx);
}

static void append_device_config_update_schema_sections(String &response,
                                                        bool wifi_saved,
                                                        bool network_saved,
                                                        bool token_saved,
                                                        bool filesystem_ready,
                                                        bool filesystem_assets_ready,
                                                        const char *filesystem_status,
                                                        const RuntimeReloadReport &runtime_reload)
{
    ApiSchemaEmitContext ctx = {};
    ctx.runtime_reload = &runtime_reload;
    ctx.wifi_saved = wifi_saved;
    ctx.network_saved = network_saved;
    ctx.token_saved = token_saved;
    ctx.filesystem_ready = filesystem_ready;
    ctx.filesystem_assets_ready = filesystem_assets_ready;
    ctx.filesystem_status = filesystem_status;
    (void)append_api_schema_sections_for_route(response, "configDevice", ctx);
}

static void append_config_device_readback_schema_sections(String &response,
                                                          const DeviceConfigSnapshot &cfg,
                                                          const StorageFacadeRuntimeSnapshot &storage_runtime)
{
    ApiSchemaEmitContext ctx = {};
    ctx.device_config = &cfg;
    ctx.storage_runtime = &storage_runtime;
    (void)append_api_schema_sections_for_route(response, "configDevice", ctx);
}

static void append_health_schema_sections(String &response)
{
    ApiSchemaEmitContext ctx = {};
    (void)append_api_schema_sections_for_route(response, "health", ctx);
}

static void append_display_frame_schema_sections(String &response)
{
    ApiSchemaEmitContext ctx = {};
    (void)append_api_schema_sections_for_route(response, "displayFrame", ctx);
}

static void append_tracked_action_ack_schema_sections(String &response,
                                                      uint32_t action_id,
                                                      WebActionType type)
{
    ApiSchemaEmitContext ctx = {};
    ctx.tracked_action_id = action_id;
    ctx.tracked_action_type = type;
    (void)append_api_schema_sections_for_route(response, "command", ctx);
}

static void send_meta_response(AsyncWebServerRequest *request)
{
    NetworkSyncSnapshot network = {};
    StorageFacadeRuntimeSnapshot storage_runtime = {};
    String response = "{";

    (void)network_sync_service_get_snapshot(&network);
    (void)storage_facade_get_runtime_snapshot(&storage_runtime);

    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    web_json_kv_u32(response, "stateVersion", WEB_STATE_VERSION, false);
    web_json_kv_u32(response, "storageSchemaVersion", APP_STORAGE_VERSION, false);
    web_json_kv_u32(response, "runtimeContractVersion", WEB_RUNTIME_CONTRACT_VERSION, false);
    web_json_kv_u32(response, "commandCatalogVersion", WEB_COMMAND_CATALOG_VERSION, false);
    web_json_kv_bool(response, "flashStorageSupported", storage_runtime.flash_supported, false);
    web_json_kv_bool(response, "flashStorageReady", storage_runtime.flash_ready, false);
    web_json_kv_bool(response, "storageMigrationAttempted", storage_runtime.migration_attempted, false);
    web_json_kv_bool(response, "storageMigrationOk", storage_runtime.migration_ok, false);
    web_json_kv_str(response, "appStateBackend", storage_runtime.app_state_backend, false);
    web_json_kv_str(response, "deviceConfigBackend", storage_runtime.device_config_backend, false);
    web_json_kv_str(response, "appStateDurability", storage_runtime.app_state_durability, false);
    web_json_kv_str(response, "deviceConfigDurability", storage_runtime.device_config_durability, false);
    web_json_kv_bool(response, "appStatePowerLossGuaranteed", storage_runtime.app_state_power_loss_guaranteed, false);
    web_json_kv_bool(response, "deviceConfigPowerLossGuaranteed", storage_runtime.device_config_power_loss_guaranteed, false);
    web_json_kv_bool(response, "appStateMixedDurability", storage_runtime.app_state_mixed_durability, false);
    web_json_kv_u32(response, "appStateResetDomainObjectCount", storage_runtime.app_state_reset_domain_object_count, false);
    web_json_kv_u32(response, "appStateDurableObjectCount", storage_runtime.app_state_durable_object_count, false);
    web_json_kv_bool(response, "authRequired", device_config_authority_has_token(), false);
    web_json_kv_bool(response, "controlLockedInProvisioningAp", control_is_locked_in_provisioning_ap(), false);
    web_json_kv_bool(response, "provisioningSerialPasswordLogEnabled", web_wifi_serial_password_log_enabled(), false);
    web_json_kv_bool(response, "webControlPlaneReady", web_server_control_plane_ready(), false);
    web_json_kv_bool(response, "webConsoleReady", web_server_console_ready(), false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
    web_json_kv_bool(response, "assetContractReady", web_server_asset_contract_ready(), false);
    web_json_kv_bool(response, "assetContractHashVerified", web_server_asset_contract_hash_verified(), false);
    web_json_kv_u32(response, "assetContractVersion", web_server_asset_contract_version(), false);
    web_json_kv_u32(response, "assetContractHash", web_server_asset_contract_hash(), false);
    web_json_kv_str(response, "assetContractGeneratedAt", web_server_asset_contract_generated_at(), false);
    web_json_kv_str(response, "assetExpectedHashIndexHtml", web_server_asset_expected_hash("index.html"), false);
    web_json_kv_str(response, "assetActualHashIndexHtml", web_server_asset_actual_hash("index.html"), false);
    web_json_kv_str(response, "assetExpectedHashAppJs", web_server_asset_expected_hash("app.js"), false);
    web_json_kv_str(response, "assetActualHashAppJs", web_server_asset_actual_hash("app.js"), false);
    web_json_kv_str(response, "assetExpectedHashAppCss", web_server_asset_expected_hash("app.css"), false);
    web_json_kv_str(response, "assetActualHashAppCss", web_server_asset_actual_hash("app.css"), false);
    web_json_kv_str(response, "assetExpectedHashContractBootstrap", web_server_asset_expected_hash("contract-bootstrap.json"), false);
    web_json_kv_str(response, "assetActualHashContractBootstrap", web_server_asset_actual_hash("contract-bootstrap.json"), false);
    web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
    web_json_kv_u32(response, "runtimeEventHandlers", runtime_event_service_handler_count(), false);
    web_json_kv_u32(response, "runtimeEventCapacity", runtime_event_service_capacity(), false);
    web_json_kv_u32(response, "runtimeEventRegistrationRejectCount", runtime_event_service_registration_reject_count(), false);
    web_json_kv_u32(response, "runtimeEventPublishCount", runtime_event_service_publish_count(), false);
    web_json_kv_u32(response, "runtimeEventPublishFailCount", runtime_event_service_publish_fail_count(), false);
    web_json_kv_u32(response, "runtimeEventLastSuccessCount", runtime_event_service_last_success_count(), false);
    web_json_kv_u32(response, "runtimeEventLastFailureCount", runtime_event_service_last_failure_count(), false);
    web_json_kv_u32(response, "runtimeEventLastCriticalFailureCount", runtime_event_service_last_critical_failure_count(), false);
    web_json_kv_str(response, "runtimeEventLast", runtime_event_service_event_name(runtime_event_service_last_event()), false);
    web_json_kv_str(response, "runtimeEventLastFailed", runtime_event_service_event_name(runtime_event_service_last_failed_event()), false);
    web_json_kv_i32(response, "runtimeEventLastFailedHandlerIndex", runtime_event_service_last_failed_handler_index(), false);
    append_meta_schema_sections(response, storage_runtime);
    response += "\"runtimeEventHandlersDetail\":[";
    for (uint8_t i = 0U; i < runtime_event_service_handler_count(); ++i) {
        if (i != 0U) {
            response += ",";
        }
        response += "{";
        web_json_kv_u32(response, "index", i, false);
        web_json_kv_str(response, "name", runtime_event_service_handler_name(i), false);
        web_json_kv_i32(response, "priority", runtime_event_service_handler_priority(i), false);
        web_json_kv_bool(response, "critical", runtime_event_service_handler_is_critical(i), true);
        response += "}";
    }
    response += "],";
    {
        SystemRuntimeCapabilities capabilities = {};
        if (system_runtime_capability_probe(&capabilities)) {
            response += "\"runtimeCapabilities\":{";
            web_json_kv_bool(response, "batteryAdc", capabilities.has_battery_adc, false);
            web_json_kv_bool(response, "vibration", capabilities.has_vibration, false);
            web_json_kv_bool(response, "sensor", capabilities.has_sensor, false);
            web_json_kv_bool(response, "watchdog", capabilities.has_watchdog, false);
            web_json_kv_bool(response, "flashStorage", capabilities.has_flash_storage, false);
            web_json_kv_bool(response, "resetDomainStorage", capabilities.has_reset_domain_storage, false);
            web_json_kv_bool(response, "fullKeypad", capabilities.has_full_keypad, false);
            web_json_kv_bool(response, "keypadMappingValid", capabilities.keypad_mapping_valid, false);
            web_json_kv_bool(response, "gpioContractValid", capabilities.gpio_contract_valid, false);
            web_json_kv_bool(response, "storageMapValid", capabilities.storage_map_valid, true);
            response += "},";
        }
    }
    response += "\"featureLifecycle\":{";
    web_json_kv_str(response, "vibration", feature_lifecycle_name(APP_FEATURE_VIBRATION != 0), false);
    web_json_kv_str(response, "batteryAdc", feature_lifecycle_name(APP_FEATURE_BATTERY != 0), false);
    web_json_kv_str(response, "watchdog", feature_lifecycle_name(APP_FEATURE_WATCHDOG != 0), false);
    web_json_kv_str(response, "companionUart", feature_lifecycle_name(APP_FEATURE_COMPANION_UART != 0), true);
    response += "},";
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
    response += "\"storageSemantics\":[";
    for (uint32_t i = 0U; i < storage_semantics_count(); ++i) {
        StorageObjectSemantic semantic = {};
        if (!storage_semantics_at(i, &semantic)) {
            continue;
        }
        if (response[response.length() - 1U] != '[') {
            response += ',';
        }
        response += '{';
        web_json_kv_u32(response, "kind", (uint32_t)semantic.kind, false);
        web_json_kv_str(response, "name", semantic.name, false);
        web_json_kv_str(response, "owner", semantic.owner, false);
        web_json_kv_str(response, "namespace", semantic.namespace_name, false);
        web_json_kv_str(response, "resetBehavior", storage_reset_behavior_name(semantic.reset_behavior), false);
        web_json_kv_str(response, "durability", storage_durability_level_name(semantic.durability), false);
        web_json_kv_bool(response, "managedByStorageService", semantic.managed_by_storage_service, false);
        web_json_kv_bool(response, "survivesPowerLoss", semantic.survives_power_loss, true);
        response += '}';
    }
    response += "]}";
    request->send(200, "application/json", response);
}

static void send_device_config_response(AsyncWebServerRequest *request)
{
    DeviceConfigSnapshot cfg;
    NetworkSyncSnapshot network = {};
    StorageFacadeRuntimeSnapshot storage_runtime = {};
    String response = "{";

    if (!device_config_authority_get(&cfg)) {
        request->send(500, "application/json", "{\"ok\":false,\"message\":\"device config unavailable\"}");
        return;
    }
    (void)network_sync_service_get_snapshot(&network);
    (void)storage_facade_get_runtime_snapshot(&storage_runtime);

    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    web_json_kv_u32(response, "stateVersion", WEB_STATE_VERSION, false);
    response += "\"config\":{";
    web_json_kv_u32(response, "version", cfg.version, false);
    web_json_kv_u32(response, "generation", device_config_generation(), false);
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
    web_json_kv_str(response, "appStateDurability", storage_runtime.app_state_durability, false);
    response += "\"featureLifecycle\":{";
    web_json_kv_str(response, "vibration", feature_lifecycle_name(APP_FEATURE_VIBRATION != 0), false);
    web_json_kv_str(response, "batteryAdc", feature_lifecycle_name(APP_FEATURE_BATTERY != 0), false);
    web_json_kv_str(response, "watchdog", feature_lifecycle_name(APP_FEATURE_WATCHDOG != 0), false);
    web_json_kv_str(response, "companionUart", feature_lifecycle_name(APP_FEATURE_COMPANION_UART != 0), true);
    response += "},";
    web_json_kv_bool(response, "weatherTlsVerified", network.weather_tls_verified, false);
    web_json_kv_bool(response, "weatherCaLoaded", network.weather_ca_loaded, false);
    web_json_kv_str(response, "weatherTlsMode", network.weather_tls_mode, false);
    web_json_kv_str(response, "weatherSyncStatus", network.weather_status, false);
    web_json_kv_i32(response, "weatherLastHttpStatus", network.last_weather_http_status, false);
    web_json_kv_str(response, "ip", web_wifi_ip_string(), true);
    response += "},";
    append_config_device_readback_schema_sections(response, cfg, storage_runtime);
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
    response += ',';
    append_actions_status_schema_sections(response, result);
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
    response += ',';
    append_actions_status_schema_sections(response, result);
    response += "}";
    request->send(200, "application/json", response);
}

static void send_state_summary_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_SUMMARY, false, 3072U);
}

static void send_state_detail_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_DETAIL, false, 6144U);
}

static void send_state_perf_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_PERF, false, 4096U);
}

static void send_state_raw_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_RAW, false, 4096U);
}

static void send_state_aggregate_response(AsyncWebServerRequest *request)
{
    send_state_payload_response(request, WEB_STATE_PAYLOAD_AGGREGATE, true, 6144U);
}

void web_register_api_routes(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_HEALTH, HTTP_GET, [](AsyncWebServerRequest *request) {
        String response = "{";
        response += "\"ok\":true,";
        web_json_kv_bool(response, "wifiConnected", web_wifi_connected(), false);
        web_json_kv_str(response, "ip", web_wifi_ip_string(), false);
        web_json_kv_u32(response, "uptimeMs", (uint32_t)millis(), false);
        web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
        web_json_kv_bool(response, "assetContractReady", web_server_asset_contract_ready() && web_server_asset_contract_hash_verified(), false);
        append_health_schema_sections(response);
        response += "}";
        request->send(200, "application/json", response);
    });

    server.on(WEB_ROUTE_CONTRACT, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_contract_response(request);
    });

    server.on(WEB_ROUTE_META, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_meta_response(request);
    });

    server.on(WEB_ROUTE_STORAGE_SEMANTICS, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_storage_semantics_response(request);
    });

    server.on(WEB_ROUTE_ACTIONS_CATALOG, HTTP_GET, [](AsyncWebServerRequest *request) {
        String response = "{";
        response += "\"ok\":true,";
        append_command_catalog(response);
        response += ',';
        append_actions_catalog_schema_sections(response);
        response += "}";
        request->send(200, "application/json", response);
    });

    server.on(WEB_ROUTE_ACTIONS_LATEST, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_latest_action_response(request);
    });

    server.on(WEB_ROUTE_ACTIONS_STATUS, HTTP_GET, [](AsyncWebServerRequest *request) {
        String id_str;
        uint32_t action_id = 0U;
        if (!read_form_field(request, "id", id_str) || !parse_strict_u32_field(id_str, &action_id) || action_id == 0U) {
            send_json_error(request, 400, "missing or invalid action id");
            return;
        }
        send_action_result_response(request, action_id);
    });


    server.on(WEB_ROUTE_CONFIG_DEVICE, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_device_config_response(request);
    });

    server.on(WEB_ROUTE_CONFIG_DEVICE, HTTP_POST, [](AsyncWebServerRequest *request) {
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

        if (!storage_facade_get_device_config(&current_cfg)) {
            send_json_error(request, 500, "device config unavailable");
            return;
        }
        if (!storage_facade_get_device_wifi_password(current_password, sizeof(current_password))) {
            send_json_error(request, 500, "device config password unavailable");
            return;
        }
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

        DeviceConfigAuthorityApplyReport apply = {};
        if (!device_config_authority_apply_update(&update, &apply) && !apply.committed) {
            send_json_error(request, 400, "invalid device config");
            return;
        }

        wifi_saved = apply.wifi_saved;
        network_saved = apply.network_saved;
        token_saved = apply.token_saved;

        String response = "{";
        response += "\"ok\":true,";
        response += "\"runtimeReload\":{";
        append_runtime_reload_payload(response, apply.reload, true);
        response += "},";
        web_json_kv_bool(response, "wifiSaved", wifi_saved, false);
        web_json_kv_bool(response, "networkSaved", network_saved, false);
        web_json_kv_bool(response, "tokenSaved", token_saved, false);
        append_runtime_reload_payload(response, apply.reload, false);
        web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
        web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
        web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
        append_device_config_update_schema_sections(response,
                                                     wifi_saved,
                                                     network_saved,
                                                     token_saved,
                                                     web_server_filesystem_ready(),
                                                     web_server_filesystem_assets_ready(),
                                                     web_server_filesystem_status(),
                                                     apply.reload);
        response += ',';
        web_json_kv_str(response,
                        "message",
                        apply.runtime_reload_requested && !apply.runtime_reload_ok ? "config saved but runtime reload failed" : "config saved",
                        true);
        response += "}";
        request->send(apply.runtime_reload_requested && !apply.runtime_reload_ok ? 202 : 200, "application/json", response);
    }, nullptr, capture_request_body);

    auto handle_device_config_reset = [](AsyncWebServerRequest *request) {
        ResetActionReport report = {};
        bool ok;
        if (!require_mutation_auth(request)) {
            return;
        }
        ok = reset_service_reset_device_config(&report);
        send_reset_action_response(request,
                                   ok,
                                   report,
                                   "device config reset",
                                   "device config reset but runtime reload failed",
                                   "device config reset failed before persistence changed");
    };

    server.on(WEB_ROUTE_STATE_DETAIL, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_state_detail_response(request);
    });

    server.on(WEB_ROUTE_STATE_PERF, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_state_perf_response(request);
    });

    server.on(WEB_ROUTE_STATE_RAW, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_state_raw_response(request);
    });

    server.on(WEB_ROUTE_STATE_AGGREGATE, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_state_aggregate_response(request);
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

    server.on(WEB_ROUTE_DISPLAY_FRAME, HTTP_GET, [](AsyncWebServerRequest *request) {
        String response;
        response.reserve(128U + (OLED_BUFFER_SIZE * 4U));
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
        response += "\",";
        append_display_frame_schema_sections(response);
        response += "}";
        request->send(200, "application/json", response);
    });

    server.on(WEB_ROUTE_INPUT_KEY, HTTP_POST, [](AsyncWebServerRequest *request) {
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

    server.on(WEB_ROUTE_COMMAND, HTTP_POST, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> json_doc;
        String cmd_str;
        String error_message;
        AppCommandType cmd_type = (AppCommandType)0;
        const StaticJsonDocument<256> *json_payload = nullptr;

        if (!require_control_auth(request)) {
            return;
        }

        if (read_form_field(request, "type", cmd_str)) {
            if (!parse_command_type(cmd_str.c_str(), &cmd_type) || cmd_type == 0) {
                send_json_error(request, 400, "invalid command");
                return;
            }
        } else if (parse_json_body(request, json_doc)) {
            json_payload = &json_doc;
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
        if (!populate_command_payload_from_request(request, json_payload, &action.data.command, cmd_str, &error_message)) {
            send_json_error(request, 400, error_message.c_str());
            return;
        }

        uint32_t action_id = 0U;
        if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
            send_json_error(request, 503, "action queue full");
            return;
        }

        send_queue_ack(request, action_id, action.type);
    }, nullptr, capture_request_body);

    server.on(WEB_ROUTE_DISPLAY_OVERLAY, HTTP_POST, [](AsyncWebServerRequest *request) {
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

    server.on(WEB_ROUTE_DISPLAY_OVERLAY_CLEAR, HTTP_POST, [](AsyncWebServerRequest *request) {
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
