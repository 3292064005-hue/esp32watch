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
#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"
#include "web/web_route_module_registry.h"
#include "web/web_routes_api_support.h"
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
#include "services/time_service.h"
}

static const char kHexDigits[] = "0123456789ABCDEF";

bool web_command_supported(const AppCommandDescriptor *descriptor)
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

const char *feature_lifecycle_name(bool enabled)
{
    return enabled ? "ACTIVE" : "ARCHIVED";
}
bool parse_key_id(const char *key_str, KeyId *out_id)
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

bool parse_key_event(const char *event_str, KeyEventType *out_event)
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

static bool web_command_legacy_alias_supported(const AppCommandDescriptor *descriptor)
{
    if (descriptor == nullptr) {
        return false;
    }
    switch (descriptor->type) {
        case APP_COMMAND_RESET_APP_STATE:
        case APP_COMMAND_FACTORY_RESET:
            return true;
        default:
            return false;
    }
}

bool parse_command_type(const char *cmd_str, AppCommandType *out_type)
{
    const AppCommandDescriptor *descriptor = app_command_describe_by_name(cmd_str);
    if (cmd_str == nullptr || out_type == nullptr || descriptor == nullptr) {
        return false;
    }
    if (!descriptor->web_exposed && !web_command_legacy_alias_supported(descriptor)) {
        return false;
    }
    *out_type = descriptor->type;
    return true;
}

void send_json_error(AsyncWebServerRequest *request, int status, const char *message)
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
    static const uint32_t kDomains[] = {
        RUNTIME_RELOAD_DOMAIN_WIFI,
        RUNTIME_RELOAD_DOMAIN_NETWORK,
        RUNTIME_RELOAD_DOMAIN_AUTH,
        RUNTIME_RELOAD_DOMAIN_DISPLAY,
        RUNTIME_RELOAD_DOMAIN_POWER,
        RUNTIME_RELOAD_DOMAIN_SENSOR,
        RUNTIME_RELOAD_DOMAIN_COMPANION,
    };

    response += '"';
    response += key;
    response += "\":";
    response += '[';
    bool appended = false;
    for (size_t i = 0; i < (sizeof(kDomains) / sizeof(kDomains[0])); ++i) {
        const uint32_t domain = kDomains[i];
        if ((domain_mask & domain) == 0U) {
            continue;
        }
        if (appended) {
            response += ',';
        }
        response += '"';
        response += runtime_reload_domain_name(domain);
        response += '"';
        appended = true;
    }
    response += ']';
    if (!last) {
        response += ',';
    }
}

static void append_runtime_reload_domain_results(String &response,
                                                 const RuntimeReloadReport &reload,
                                                 bool last)
{
    response += "\"runtimeReloadDomainResults\":";
    response += '[';
    for (uint8_t i = 0U; i < reload.domain_result_count; ++i) {
        const RuntimeReloadDomainResult &result = reload.domain_results[i];
        if (i != 0U) {
            response += ',';
        }
        response += '{';
        web_json_kv_str(response, "domain", result.domain_name != NULL ? result.domain_name : runtime_reload_domain_name(result.domain_mask), false);
        web_json_kv_u32(response, "domainMask", result.domain_mask, false);
        web_json_kv_bool(response, "requested", result.requested, false);
        web_json_kv_bool(response, "supported", result.supported, false);
        web_json_kv_str(response, "applyStrategy", result.apply_strategy != NULL ? result.apply_strategy : "UNKNOWN", false);
        web_json_kv_bool(response, "dispatchMatched", result.dispatch_matched, false);
        web_json_kv_bool(response, "applied", result.applied, false);
        web_json_kv_bool(response, "persistedOnly", result.persisted_only, false);
        web_json_kv_bool(response, "rebootRequired", result.reboot_required, false);
        web_json_kv_bool(response, "effective", result.effective, false);
        web_json_kv_bool(response, "verifyOk", result.verify_ok, false);
        web_json_kv_u32(response, "appliedGeneration", result.applied_generation, false);
        web_json_kv_str(response, "verifyReason", result.verify_reason != NULL ? result.verify_reason : "NONE", true);
        response += '}';
    }
    response += ']';
    if (!last) {
        response += ',';
    }
}

void append_runtime_reload_payload(String &response, const RuntimeReloadReport &reload, bool append_trailing)
{
    const uint32_t supported_domain_mask = runtime_reload_supported_domain_mask();
    const uint32_t requested_supported_mask = reload.requested_domain_mask & supported_domain_mask;
    const uint32_t effective_domain_mask = reload.applied_domain_mask |
                                           reload.deferred_domain_mask |
                                           reload.reboot_required_domain_mask;
    const bool runtime_reloaded = (!reload.requested) ||
                                  (requested_supported_mask == RUNTIME_RELOAD_DOMAIN_NONE) ||
                                  (reload.failed_domain_mask == RUNTIME_RELOAD_DOMAIN_NONE &&
                                   requested_supported_mask == effective_domain_mask &&
                                   reload.verify_ok);

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
    web_json_kv_u32(response, "runtimeMatchedHandlerCount", reload.matched_handler_count, false);
    web_json_kv_u32(response, "runtimeHandlerSuccessCount", reload.handler_success_count, false);
    web_json_kv_u32(response, "runtimeHandlerFailureCount", reload.handler_failure_count, false);
    web_json_kv_u32(response, "runtimeHandlerCriticalFailureCount", reload.handler_critical_failure_count, false);
    web_json_kv_u32(response, "runtimeReloadConfigGeneration", reload.config_generation, false);
    web_json_kv_u32(response, "runtimeWifiAppliedGeneration", reload.wifi_applied_generation, false);
    web_json_kv_u32(response, "runtimeNetworkAppliedGeneration", reload.network_applied_generation, false);
    web_json_kv_bool(response, "runtimeReloadRequiresReboot", reload.requires_reboot, false);
    web_json_kv_u32(response, "runtimeReloadDomainResultCount", reload.domain_result_count, false);
    web_json_kv_str(response, "runtimeWifiVerifyReason", reload.wifi_verify_reason != NULL ? reload.wifi_verify_reason : "NONE", false);
    web_json_kv_str(response, "runtimeNetworkVerifyReason", reload.network_verify_reason != NULL ? reload.network_verify_reason : "NONE", false);
    append_runtime_reload_domain_array(response, "runtimeReloadSupportedDomains", supported_domain_mask, false);
    append_runtime_reload_domain_array(response, "runtimeReloadImpactDomains", reload.requested_domain_mask, false);
    append_runtime_reload_domain_array(response, "runtimeReloadAppliedDomains", reload.applied_domain_mask, false);
    append_runtime_reload_domain_array(response, "runtimeReloadDeferredDomains", reload.deferred_domain_mask, false);
    append_runtime_reload_domain_array(response, "runtimeReloadRebootRequiredDomains", reload.reboot_required_domain_mask, false);
    append_runtime_reload_domain_array(response, "runtimeReloadFailedDomains", reload.failed_domain_mask, false);
    append_runtime_reload_domain_results(response, reload, false);
    web_json_kv_str(response, "runtimeReloadFailurePhase", reload.failure_phase != NULL ? reload.failure_phase : "NONE", false);
    web_json_kv_str(response, "runtimeReloadFailureCode", reload.failure_code != NULL ? reload.failure_code : "NONE", append_trailing);
}

void append_time_authority_fields(String &response, bool append_trailing)
{
    TimeSourceSnapshot source_snapshot = {};
    const bool has_snapshot = time_service_get_source_snapshot(&source_snapshot);
    web_json_kv_str(response,
                    "timeAuthority",
                    time_service_authority_name(has_snapshot ? source_snapshot.authority : TIME_AUTHORITY_NONE),
                    false);
    web_json_kv_str(response,
                    "timeSource",
                    time_service_source_name(has_snapshot ? source_snapshot.source : TIME_SOURCE_RECOVERY),
                    false);
    web_json_kv_str(response,
                    "timeConfidence",
                    time_service_confidence_name(has_snapshot ? source_snapshot.confidence : TIME_CONFIDENCE_NONE),
                    append_trailing);
}

void append_reset_report_payload(String &response, const ResetActionReport &report, bool append_trailing)
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

void send_reset_action_response(AsyncWebServerRequest *request,
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

bool read_json_body(AsyncWebServerRequest *request, String &body)
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

bool read_form_field(AsyncWebServerRequest *request, const char *name, String &value)
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

bool parse_strict_float_field(const String &value, float *out_value)
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

bool parse_strict_u32_field(const String &value, uint32_t *out_value)
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

bool populate_command_payload_from_request(AsyncWebServerRequest *request,
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

void capture_request_body(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
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

void append_hex_byte(String &out, uint8_t value)
{
    out += kHexDigits[(value >> 4) & 0x0F];
    out += kHexDigits[value & 0x0F];
}

bool auth_token_from_request(AsyncWebServerRequest *request, String &token)
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

bool request_is_authorized(AsyncWebServerRequest *request)
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

bool control_is_locked_in_provisioning_ap(void)
{
    return !device_config_authority_has_token() && web_wifi_access_point_active() && !web_wifi_connected();
}

bool require_mutation_auth(AsyncWebServerRequest *request)
{
    if (request_is_authorized(request)) {
        return true;
    }
    request->send(401, "application/json", "{\"ok\":false,\"message\":\"unauthorized\"}");
    return false;
}

bool require_control_auth(AsyncWebServerRequest *request)
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
