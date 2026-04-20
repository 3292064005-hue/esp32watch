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

void append_action_status_payload(String &response, const WebActionStatusSnapshot &status, bool append_ok_prefix)
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

void send_queue_ack(AsyncWebServerRequest *request, uint32_t action_id, WebActionType type)
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

static void append_command_catalog_entry(String &response,
                                         const char *type,
                                         bool web_exposed,
                                         const char *lifecycle,
                                         const char *canonical_type,
                                         bool legacy_alias)
{
    if (response[response.length() - 1U] != '[') {
        response += ',';
    }
    response += '{';
    web_json_kv_str(response, "type", type != nullptr ? type : "UNKNOWN", false);
    web_json_kv_bool(response, "webExposed", web_exposed, false);
    web_json_kv_str(response, "lifecycle", lifecycle != nullptr ? lifecycle : "ACTIVE", false);
    web_json_kv_str(response, "canonicalType", canonical_type != nullptr ? canonical_type : type, false);
    web_json_kv_bool(response, "legacyAlias", legacy_alias, true);
    response += '}';
}

void append_command_catalog(String &response)
{
    static const struct {
        const char *alias_type;
        const char *canonical_type;
    } kDeprecatedAliases[] = {
        {"resetAppState", "resetAppState"},
        {"factoryReset", "factoryReset"},
        {"restoreDefaults", "resetAppState"},
    };

    size_t i;
    response += "\"commandCatalogVersion\":";
    response += WEB_COMMAND_CATALOG_VERSION;
    response += ",\"commands\":[";
    for (i = 0; i < app_command_catalog_count(); ++i) {
        const AppCommandDescriptor *descriptor = app_command_catalog_at(i);
        if (descriptor == nullptr || !descriptor->web_exposed || !web_command_supported(descriptor)) {
            continue;
        }
        append_command_catalog_entry(response, descriptor->wire_name, descriptor->web_exposed, "ACTIVE", descriptor->wire_name, false);
    }
    for (i = 0; i < (sizeof(kDeprecatedAliases) / sizeof(kDeprecatedAliases[0])); ++i) {
        append_command_catalog_entry(response,
                                     kDeprecatedAliases[i].alias_type,
                                     false,
                                     "DEPRECATED",
                                     kDeprecatedAliases[i].canonical_type,
                                     true);
    }
    response += ']';
}

void append_command_catalog_section(String &response, bool append_trailing)
{
    response += "\"commandCatalog\":{";
    append_command_catalog(response);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

void append_action_status_section(String &response, const WebActionStatusSnapshot &status, bool append_trailing)
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

void append_runtime_reload_section(String &response, const RuntimeReloadReport &reload, bool append_trailing)
{
    response += "\"runtimeReload\":{";
    append_runtime_reload_payload(response, reload, true);
    response += '}';
    if (!append_trailing) {
        response += ',';
    }
}

void append_reset_action_section(String &response, const ResetActionReport &report, bool append_trailing)
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

void append_device_config_update_section(String &response,
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

void append_capabilities(String &response)
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

void append_state_surface_catalog(String &response)
{
    response += "\"stateSurfaces\":{";
    response += "\"controlPlane\":[\"stateAggregate\"],";
    response += "\"diagnostics\":[\"stateDetail\",\"statePerf\",\"storageSemantics\"],";
    response += "\"compatibility\":[\"stateSummary\"],";
    response += "\"internal\":[\"stateRaw\"]";
    response += "}";
}


// collect_api_state_bundle implemented in web_route_state_payload.cpp
// append_state_versions(response); emitted by web_route_state_payload.cpp
// state payload assembly moved out of this file to bounded-context state implementation

void web_send_contract_response(AsyncWebServerRequest *request)
{
    String response = "{";
    response += "\"ok\":true,";
    web_contract_append_json(response);
    response += "}";
    request->send(200, "application/json", response);
}

static void append_storage_backend_capabilities(String &response)
{
    StorageBackendCapabilities capabilities = {};
    response += "{";
    if (!storage_service_get_backend_capabilities(&capabilities)) {
        web_json_kv_bool(response, "available", false, true);
        response += "}";
        return;
    }
    web_json_kv_bool(response, "available", true, false);
    web_json_kv_str(response, "backend", capabilities.backend_name, false);
    web_json_kv_bool(response, "flashReady", capabilities.flash_ready, false);
    web_json_kv_bool(response, "bkpReady", capabilities.bkp_ready, false);
    web_json_kv_bool(response, "appStateDurableReady", capabilities.app_state_durable_ready, false);
    web_json_kv_bool(response, "powerLossGuaranteed", capabilities.power_loss_guaranteed, false);
    web_json_kv_bool(response, "supportsAtomicCommit", capabilities.supports_atomic_commit, false);
    web_json_kv_bool(response, "supportsRollbackAbort", capabilities.supports_rollback_abort, false);
    web_json_kv_u32(response, "maxCommitTicks", capabilities.max_commit_ticks, false);
    web_json_kv_str(response, "atomicity", storage_service_backend_atomicity_name(capabilities.atomicity), false);
    web_json_kv_str(response, "verifyMode", storage_service_backend_verify_mode_name(capabilities.verify_mode), false);
    web_json_kv_str(response, "latencyClass", storage_service_backend_latency_name(capabilities.latency_class), true);
    response += "}";
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

void web_send_storage_semantics_response(AsyncWebServerRequest *request)
{
    String response = "{";
    response += "\"ok\":true,";
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    response += "\"objects\":";
    append_storage_semantics_array(response);
    response += ",\"storageSemantics\":";
    append_storage_semantics_array(response);
    response += ",\"backendCapabilities\":";
    append_storage_backend_capabilities(response);
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
    web_json_kv_u32(response, "appStateDurableObjectCount", storage_runtime.app_state_durable_object_count, false);
    response += "\"backendCapabilities\":";
    append_storage_backend_capabilities(response);
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
        web_json_kv_bool(response, "rtcWallClock", platform_support.rtc_wall_clock_supported, false);
        web_json_kv_bool(response, "rtcWallClockPersistent", platform_support.rtc_wall_clock_persistent, false);
        web_json_kv_bool(response, "idleLightSleep", platform_support.idle_light_sleep_supported, false);
        web_json_kv_bool(response, "watchdog", platform_support.watchdog_supported, false);
        web_json_kv_bool(response, "flashJournal", platform_support.flash_journal_supported, true);
    } else {
        web_json_kv_bool(response, "rtcResetDomain", false, false);
        web_json_kv_bool(response, "rtcWallClock", false, false);
        web_json_kv_bool(response, "rtcWallClockPersistent", false, false);
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

static void append_meta_state_surfaces_section(String &response, bool append_trailing)
{
    append_state_surface_catalog(response);
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
    response += ",\"backendCapabilities\":";
    append_storage_backend_capabilities(response);
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
    append_time_authority_fields(response, false);
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
    if (strcmp(section, "stateSurfaces") == 0) {
        append_meta_state_surfaces_section(response, append_trailing);
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

void append_actions_catalog_schema_sections(String &response)
{
    ApiSchemaEmitContext ctx = {};
    (void)append_api_schema_sections_for_route(response, "actionsCatalog", ctx);
}

void append_actions_status_schema_sections(String &response, const WebActionStatusSnapshot &result)
{
    ApiSchemaEmitContext ctx = {};
    ctx.action_status = &result;
    (void)append_api_schema_sections_for_route(response, "actionsStatus", ctx);
}

void append_reset_action_schema_sections(String &response, const ResetActionReport &report)
{
    ApiSchemaEmitContext ctx = {};
    ctx.reset_report = &report;
    ctx.runtime_reload = &report.reload;
    (void)append_api_schema_sections_for_route(response, "resetDeviceConfig", ctx);
}

void append_device_config_update_schema_sections(String &response,
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

void append_config_device_readback_schema_sections(String &response,
                                                   const DeviceConfigSnapshot &cfg,
                                                   const StorageFacadeRuntimeSnapshot &storage_runtime)
{
    ApiSchemaEmitContext ctx = {};
    ctx.device_config = &cfg;
    ctx.storage_runtime = &storage_runtime;
    (void)append_api_schema_sections_for_route(response, "configDevice", ctx);
}

void append_health_schema_sections(String &response)
{
    ApiSchemaEmitContext ctx = {};
    (void)append_api_schema_sections_for_route(response, "health", ctx);
}

void append_display_frame_schema_sections(String &response)
{
    ApiSchemaEmitContext ctx = {};
    (void)append_api_schema_sections_for_route(response, "displayFrame", ctx);
}

void append_tracked_action_ack_schema_sections(String &response,
                                                      uint32_t action_id,
                                                      WebActionType type)
{
    ApiSchemaEmitContext ctx = {};
    ctx.tracked_action_id = action_id;
    ctx.tracked_action_type = type;
    (void)append_api_schema_sections_for_route(response, "command", ctx);
}

void web_send_meta_response(AsyncWebServerRequest *request)
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
    append_time_authority_fields(response, false);
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
