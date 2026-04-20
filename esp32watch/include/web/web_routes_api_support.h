#ifndef WEB_ROUTES_API_SUPPORT_H
#define WEB_ROUTES_API_SUPPORT_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "web/web_action_queue.h"
#include <cmath>

extern "C" {
#include "key.h"
#include "app_command.h"
#include "services/runtime_reload_coordinator.h"
#include "services/reset_service.h"
#include "services/storage_facade.h"
}

bool web_command_supported(const AppCommandDescriptor *descriptor);
const char *feature_lifecycle_name(bool enabled);
bool parse_key_id(const char *key_str, KeyId *out_id);
bool parse_key_event(const char *event_str, KeyEventType *out_event);
bool parse_command_type(const char *cmd_str, AppCommandType *out_type);
void send_json_error(AsyncWebServerRequest *request, int status, const char *message);
void append_runtime_reload_payload(String &response, const RuntimeReloadReport &reload, bool append_trailing);
void append_time_authority_fields(String &response, bool append_trailing);
void append_reset_report_payload(String &response, const ResetActionReport &report, bool append_trailing);
bool read_json_body(AsyncWebServerRequest *request, String &body);

template <size_t N>
bool parse_json_body(AsyncWebServerRequest *request, StaticJsonDocument<N> &doc, String *out_body = nullptr)
{
    String body;
    if (!read_json_body(request, body)) {
        return false;
    }
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        return false;
    }
    if (out_body != nullptr) {
        *out_body = body;
    }
    return true;
}

bool read_form_field(AsyncWebServerRequest *request, const char *name, String &value);
bool parse_strict_float_field(const String &value, float *out_value);
bool parse_strict_u32_field(const String &value, uint32_t *out_value);

template <typename TRoot>
bool read_json_string_field(const TRoot &root, const char *name, String &value)
{
    auto field = root[name];
    if (field.isNull() || !field.template is<const char *>()) {
        return false;
    }
    value = field.template as<const char *>();
    return true;
}

template <typename TRoot>
bool read_json_u32_field(const TRoot &root, const char *name, uint32_t *value)
{
    auto field = root[name];
    if (value == nullptr || field.isNull() || !field.template is<uint32_t>()) {
        return false;
    }
    *value = field.template as<uint32_t>();
    return true;
}

template <typename TRoot>
bool read_json_float_field(const TRoot &root, const char *name, float *value)
{
    auto field = root[name];
    if (value == nullptr || field.isNull() || !field.template is<float>()) {
        return false;
    }
    *value = field.template as<float>();
    return std::isfinite((double)(*value));
}

template <typename TRoot>
bool read_json_bool_field(const TRoot &root, const char *name, bool *value)
{
    auto field = root[name];
    if (value == nullptr || field.isNull() || !field.template is<bool>()) {
        return false;
    }
    *value = field.template as<bool>();
    return true;
}

bool populate_command_payload_from_request(AsyncWebServerRequest *request,
                                           const StaticJsonDocument<256> *json_doc,
                                           AppCommand *command,
                                           const String &cmd_str,
                                           String *out_error);
void capture_request_body(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
bool auth_token_from_request(AsyncWebServerRequest *request, String &token);
bool request_is_authorized(AsyncWebServerRequest *request);
bool require_mutation_auth(AsyncWebServerRequest *request);
bool require_control_auth(AsyncWebServerRequest *request);
void append_action_status_payload(String &response, const WebActionStatusSnapshot &status, bool append_ok_prefix);
void send_queue_ack(AsyncWebServerRequest *request, uint32_t action_id, WebActionType type);
void append_hex_byte(String &out, uint8_t value);
bool control_is_locked_in_provisioning_ap(void);
void send_reset_action_response(AsyncWebServerRequest *request,
                                bool ok,
                                const ResetActionReport &report,
                                const char *success_message,
                                const char *partial_failure_message,
                                const char *hard_failure_message);
void append_command_catalog(String &response);
void append_command_catalog_section(String &response, bool append_trailing);
void append_action_status_section(String &response, const WebActionStatusSnapshot &status, bool append_trailing);
void append_runtime_reload_section(String &response, const RuntimeReloadReport &reload, bool append_trailing);
void append_reset_action_section(String &response, const ResetActionReport &report, bool append_trailing);
void append_device_config_update_section(String &response,
                                         bool wifi_saved,
                                         bool network_saved,
                                         bool token_saved,
                                         bool filesystem_ready,
                                         bool filesystem_assets_ready,
                                         const char *filesystem_status,
                                         bool append_trailing);
void append_capabilities(String &response);
void append_state_surface_catalog(String &response);
void append_actions_catalog_schema_sections(String &response);
void append_actions_status_schema_sections(String &response, const WebActionStatusSnapshot &result);
void append_reset_action_schema_sections(String &response, const ResetActionReport &report);
void append_device_config_update_schema_sections(String &response,
                                                 bool wifi_saved,
                                                 bool network_saved,
                                                 bool token_saved,
                                                 bool filesystem_ready,
                                                 bool filesystem_assets_ready,
                                                 const char *filesystem_status,
                                                 const RuntimeReloadReport &runtime_reload);
void append_config_device_readback_schema_sections(String &response,
                                                   const DeviceConfigSnapshot &cfg,
                                                   const StorageFacadeRuntimeSnapshot &storage_runtime);
void append_health_schema_sections(String &response);
void append_display_frame_schema_sections(String &response);
void append_tracked_action_ack_schema_sections(String &response, uint32_t action_id, WebActionType type);

#endif
