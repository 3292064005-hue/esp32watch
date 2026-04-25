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

void web_send_device_config_response(AsyncWebServerRequest *request)
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

void web_send_action_status_response(AsyncWebServerRequest *request, uint32_t action_id)
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

void web_send_latest_action_response(AsyncWebServerRequest *request)
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


void web_handle_app_state_reset_common(AsyncWebServerRequest *request)
{
    ResetActionReport report = {};
    bool ok;

    if (!require_mutation_auth(request)) {
        return;
    }

    ok = reset_service_reset_app_state(&report);
    send_reset_action_response(request,
                               ok,
                               report,
                               "app state reset",
                               "app state reset completed with runtime follow-up warnings",
                               "app state reset failed before persistence changed");
}

void web_handle_device_config_reset_common(AsyncWebServerRequest *request)
{
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
}

void web_handle_factory_reset_common(AsyncWebServerRequest *request)
{
    ResetActionReport report = {};
    bool ok;

    if (!require_mutation_auth(request)) {
        return;
    }

    ok = reset_service_factory_reset(&report);
    send_reset_action_response(request,
                               ok,
                               report,
                               "factory reset",
                               "factory reset completed but runtime reload failed",
                               "factory reset failed before persistence changed");
}




static uint32_t runtime_reload_domain_mask_from_name(const char *name)
{
    if (name == nullptr || name[0] == '\0') {
        return RUNTIME_RELOAD_DOMAIN_NONE;
    }
    if (strcasecmp(name, "wifi") == 0) return RUNTIME_RELOAD_DOMAIN_WIFI;
    if (strcasecmp(name, "network") == 0) return RUNTIME_RELOAD_DOMAIN_NETWORK;
    if (strcasecmp(name, "auth") == 0) return RUNTIME_RELOAD_DOMAIN_AUTH;
    if (strcasecmp(name, "display") == 0) return RUNTIME_RELOAD_DOMAIN_DISPLAY;
    if (strcasecmp(name, "power") == 0) return RUNTIME_RELOAD_DOMAIN_POWER;
    if (strcasecmp(name, "sensor") == 0) return RUNTIME_RELOAD_DOMAIN_SENSOR;
    if (strcasecmp(name, "companion") == 0) return RUNTIME_RELOAD_DOMAIN_COMPANION;
    return RUNTIME_RELOAD_DOMAIN_NONE;
}

static bool parse_runtime_reload_domains_csv(const String &value, uint32_t *out_mask)
{
    const char *cursor = value.c_str();
    uint32_t mask = RUNTIME_RELOAD_DOMAIN_NONE;
    if (out_mask == nullptr) {
        return false;
    }
    while (cursor != nullptr && *cursor != '\0') {
        while (*cursor == ' ' || *cursor == '	' || *cursor == ',') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *token_end = cursor;
        while (*token_end != '\0' && *token_end != ',') {
            ++token_end;
        }
        size_t token_len = (size_t)(token_end - cursor);
        char token[24] = {0};
        uint32_t bit;
        if (token_len == 0U || token_len >= sizeof(token)) {
            return false;
        }
        memcpy(token, cursor, token_len);
        token[token_len] = '\0';
        bit = runtime_reload_domain_mask_from_name(token);
        if (bit == RUNTIME_RELOAD_DOMAIN_NONE) {
            return false;
        }
        mask |= bit;
        cursor = token_end;
    }
    *out_mask = mask;
    return true;
}

template <typename TRoot>
static bool read_json_runtime_reload_domains_field(const TRoot &root, const char *name, uint32_t *out_mask)
{
    auto field = root[name];
    uint32_t mask = RUNTIME_RELOAD_DOMAIN_NONE;
    if (out_mask == nullptr || field.isNull() || !field.template is<const char *>()) {
        return false;
    }
    if (!parse_runtime_reload_domains_csv(field.template as<const char *>(), &mask)) {
        return false;
    }
    *out_mask = mask;
    return true;
}

void web_send_health_response(AsyncWebServerRequest *request)
{
    String response = "{";
    response += "\"ok\":true,";
    web_json_kv_bool(response, "wifiConnected", web_wifi_connected(), false);
    web_json_kv_str(response, "ip", web_wifi_ip_string(), false);
    web_json_kv_u32(response, "uptimeMs", (uint32_t)millis(), false);
    append_time_authority_fields(response, false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "assetContractReady", web_server_asset_contract_ready() && web_server_asset_contract_hash_verified(), false);
    append_health_schema_sections(response);
    response += "}";
    request->send(200, "application/json", response);
}

void web_send_actions_catalog_response(AsyncWebServerRequest *request)
{
    String response = "{";
    response += "\"ok\":true,";
    append_command_catalog(response);
    response += ',';
    append_actions_catalog_schema_sections(response);
    response += "}";
    request->send(200, "application/json", response);
}

void web_send_action_status_response(AsyncWebServerRequest *request)
{
    String id_str;
    uint32_t action_id = 0U;
    if (!read_form_field(request, "id", id_str) || !parse_strict_u32_field(id_str, &action_id) || action_id == 0U) {
        send_json_error(request, 400, "missing or invalid action id");
        return;
    }
    web_send_action_status_response(request, action_id);
}

void web_handle_config_device_post_common(AsyncWebServerRequest *request)
{
    DeviceConfigSnapshot current_cfg;
    DeviceConfigUpdate update = {};
    DeviceConfigAuthorityApplyReport apply = {};
    StaticJsonDocument<512> json_doc;
    String raw_body;
    String ssid;
    String password;
    String timezone_posix;
    String timezone_id;
    String location_name;
    String api_token;
    String runtime_reload_domains_csv;
    char current_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
    float latitude = 0.0f;
    float longitude = 0.0f;
    uint32_t explicit_runtime_reload_mask = RUNTIME_RELOAD_DOMAIN_NONE;
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
    bool has_runtime_reload_domains_field = false;

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
        has_runtime_reload_domains_field = read_json_runtime_reload_domains_field(json_doc, "runtimeReloadDomains", &explicit_runtime_reload_mask);
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
        has_runtime_reload_domains_field = read_form_field(request, "runtimeReloadDomains", runtime_reload_domains_csv);
        if (has_latitude_field && !parse_strict_float_field(latitude_str, &latitude)) {
            send_json_error(request, 400, "invalid latitude");
            return;
        }
        if (has_longitude_field && !parse_strict_float_field(longitude_str, &longitude)) {
            send_json_error(request, 400, "invalid longitude");
            return;
        }
        if (has_runtime_reload_domains_field && !parse_runtime_reload_domains_csv(runtime_reload_domains_csv, &explicit_runtime_reload_mask)) {
            send_json_error(request, 400, "invalid runtimeReloadDomains");
            return;
        }
    }

    if (!has_ssid_field && !has_password_field && !has_timezone_posix_field && !has_timezone_id_field &&
        !has_latitude_field && !has_longitude_field && !has_location_field && !has_token_field &&
        !has_runtime_reload_domains_field) {
        send_json_error(request, 400, "no config fields");
        return;
    }

    if (has_ssid_field || has_password_field) {
        update.set_wifi = true;
        strncpy(update.wifi_ssid, has_ssid_field ? ssid.c_str() : current_cfg.wifi_ssid, sizeof(update.wifi_ssid) - 1U);
        strncpy(update.wifi_password, has_password_field ? password.c_str() : current_password, sizeof(update.wifi_password) - 1U);
    }
    if (has_timezone_posix_field || has_timezone_id_field || has_latitude_field || has_longitude_field || has_location_field) {
        update.set_network = true;
        strncpy(update.timezone_posix, has_timezone_posix_field ? timezone_posix.c_str() : current_cfg.timezone_posix, sizeof(update.timezone_posix) - 1U);
        strncpy(update.timezone_id, has_timezone_id_field ? timezone_id.c_str() : current_cfg.timezone_id, sizeof(update.timezone_id) - 1U);
        strncpy(update.location_name, has_location_field ? location_name.c_str() : current_cfg.location_name, sizeof(update.location_name) - 1U);
        update.latitude = latitude;
        update.longitude = longitude;
    }
    if (has_token_field) {
        update.set_api_token = true;
        strncpy(update.api_token, api_token.c_str(), sizeof(update.api_token) - 1U);
    }
    if (has_runtime_reload_domains_field) {
        update.request_runtime_reload = true;
        update.runtime_reload_domain_mask = explicit_runtime_reload_mask;
    }

    if (!device_config_authority_apply_update(&update, &apply) && !apply.committed) {
        send_json_error(request, 400, "invalid device config");
        return;
    }

    String response = "{";
    response += "\"ok\":true,";
    web_json_kv_bool(response, "wifiSaved", apply.wifi_saved, false);
    web_json_kv_bool(response, "networkSaved", apply.network_saved, false);
    web_json_kv_bool(response, "tokenSaved", apply.token_saved, false);
    append_runtime_reload_payload(response, apply.reload, false);
    web_json_kv_bool(response, "filesystemReady", web_server_filesystem_ready(), false);
    web_json_kv_bool(response, "filesystemAssetsReady", web_server_filesystem_assets_ready(), false);
    web_json_kv_str(response, "filesystemStatus", web_server_filesystem_status(), false);
    append_device_config_update_schema_sections(response,
                                               apply.wifi_saved,
                                               apply.network_saved,
                                               apply.token_saved,
                                               web_server_filesystem_ready(),
                                               web_server_filesystem_assets_ready(),
                                               web_server_filesystem_status(),
                                               apply.reload);
    response += ',';
    web_json_kv_str(response, "message", apply.runtime_reload_requested && !apply.runtime_reload_ok ? "config saved but runtime reload failed" : "config saved", true);
    response += "}";
    request->send(apply.runtime_reload_requested && !apply.runtime_reload_ok ? 202 : 200, "application/json", response);
}
