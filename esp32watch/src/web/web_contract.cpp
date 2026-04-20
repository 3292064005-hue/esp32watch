#include "web/web_contract.h"
#include "web/web_json.h"
#include <Arduino.h>
#include <string.h>

namespace {

struct StateSchemaEntry {
    const char *section;
    const char *const *flags;
    size_t flag_count;
};

struct ApiSchemaSpec {
    const char *name;
    const char *const *required;
    size_t required_count;
    const char *const *sections;
    size_t section_count;
};

struct RouteSchemaSpec {
    const char *route_key;
    const char *schema_kind;
    const char *schema_name;
    const char *surface_tier;
    const char *producer_owner;
    const char *consumer_owner;
    const char *deprecation_policy;
    const char *const *request_fields;
    size_t request_field_count;
    const char *const *response_fields;
    size_t response_field_count;
};

static void append_route(String &response, const char *key, const char *value, bool trailing)
{
    web_json_kv_str(response, key, value, trailing);
}

static void append_string_array(String &response, const char *const *values, size_t count)
{
    response += '[';
    for (size_t i = 0; i < count; ++i) {
        if (i != 0U) {
            response += ',';
        }
        response += '"';
        web_json_escape_append(response, values[i]);
        response += '"';
    }
    response += ']';
}

static void append_state_schema_entries(String &response,
                                        const StateSchemaEntry *entries,
                                        size_t count)
{
    response += '[';
    for (size_t i = 0; i < count; ++i) {
        if (i != 0U) {
            response += ',';
        }
        response += '{';
        web_json_kv_str(response, "section", entries[i].section, false);
        response += "\"flags\":";
        append_string_array(response, entries[i].flags, entries[i].flag_count);
        response += '}';
    }
    response += ']';
}

static void append_api_schema(String &response, const ApiSchemaSpec &schema, bool trailing)
{
    response += '"';
    response += schema.name;
    response += "\":{";
    web_json_kv_str(response, "type", "object", false);
    response += "\"required\":";
    append_string_array(response, schema.required, schema.required_count);
    response += ',';
    response += "\"sections\":";
    append_string_array(response, schema.sections, schema.section_count);
    response += '}';
    if (!trailing) {
        response += ',';
    }
}

static constexpr const char *kFlagWifiRssi[] = {"WEB_STATE_SECTION_FLAG_WIFI_INCLUDE_RSSI"};
static constexpr const char *kFlagSystemAggregate[] = {
    "WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_APP_INIT_STAGE",
    "WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SLEEP_STATE",
    "WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SAFE_MODE",
};
static constexpr const char *kFlagSummaryNetwork[] = {"WEB_STATE_SECTION_FLAG_SUMMARY_INCLUDE_NETWORK_LABEL"};
static constexpr const char *kFlagDiagSafeMode[] = {"WEB_STATE_SECTION_FLAG_DIAG_INCLUDE_SAFE_MODE_FIELDS"};
static constexpr const char *kFlagWeatherUpdated[] = {"WEB_STATE_SECTION_FLAG_WEATHER_INCLUDE_UPDATED_AT"};
static constexpr const char *kFlagPerfHistory[] = {"WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY"};
static constexpr const char *kFlagSensorRawAxes[] = {"WEB_STATE_SECTION_FLAG_SENSOR_INCLUDE_RAW_AXES"};

static constexpr StateSchemaEntry kStateSchemaSummary[] = {
    {"wifi", nullptr, 0U},
    {"system", nullptr, 0U},
    {"summary", kFlagSummaryNetwork, 1U},
    {"weather", nullptr, 0U},
};

static constexpr StateSchemaEntry kStateSchemaDetail[] = {
    {"activity", nullptr, 0U},
    {"alarm", nullptr, 0U},
    {"music", nullptr, 0U},
    {"sensor", nullptr, 0U},
    {"storage", nullptr, 0U},
    {"diag", kFlagDiagSafeMode, 1U},
    {"display", nullptr, 0U},
    {"weather", kFlagWeatherUpdated, 1U},
    {"terminal", nullptr, 0U},
    {"overlay", nullptr, 0U},
    {"perf", kFlagPerfHistory, 1U},
};

static constexpr StateSchemaEntry kStateSchemaPerf[] = {
    {"perf", kFlagPerfHistory, 1U},
};

static constexpr StateSchemaEntry kStateSchemaRaw[] = {
    {"startupRaw", nullptr, 0U},
    {"queueRaw", nullptr, 0U},
    {"sensor", kFlagSensorRawAxes, 1U},
    {"storage", nullptr, 0U},
    {"display", nullptr, 0U},
    {"perf", kFlagPerfHistory, 1U},
};

static constexpr StateSchemaEntry kStateSchemaAggregate[] = {
    {"wifi", kFlagWifiRssi, 1U},
    {"system", kFlagSystemAggregate, 3U},
    {"config", nullptr, 0U},
    {"activity", nullptr, 0U},
    {"alarm", nullptr, 0U},
    {"music", nullptr, 0U},
    {"sensor", nullptr, 0U},
    {"storage", nullptr, 0U},
    {"diag", nullptr, 0U},
    {"display", nullptr, 0U},
    {"weather", kFlagWeatherUpdated, 1U},
    {"summary", nullptr, 0U},
    {"terminal", nullptr, 0U},
    {"overlay", nullptr, 0U},
    {"perf", nullptr, 0U},
};


static constexpr const char *kNoRouteFields[] = {};
static constexpr const char *kConfigDeviceRequestFields[] = {"ssid", "password", "timezonePosix", "timezoneId", "latitude", "longitude", "locationName", "apiToken", "runtimeReloadDomains"};
static constexpr const char *kStateRouteResponseFields[] = {"ok", "apiVersion", "stateVersion", "stateRevision"};
static constexpr const char *kTrackedActionResponseFields[] = {"ok", "actionId", "requestId", "actionType", "trackPath", "queueDepth"};
static constexpr const char *kResetResponseFields[] = {"ok", "message", "resetKind", "runtimeReload"};
static constexpr const char *kDisplayFrameResponseFields[] = {"ok", "width", "height", "presentCount", "bufferHex"};
static constexpr const char *kHealthResponseFields[] = {"ok", "wifiConnected", "ip", "uptimeMs", "timeAuthority", "timeSource", "timeConfidence", "filesystemReady", "assetContractReady"};
static constexpr const char *kMetaResponseFields[] = {"ok", "apiVersion", "stateVersion", "storageSchemaVersion", "timeAuthority", "timeSource", "timeConfidence"};
static constexpr const char *kActionsCatalogResponseFields[] = {"ok", "commands"};
static constexpr const char *kActionsStatusResponseFields[] = {"ok", "id", "status", "type"};
static constexpr const char *kContractResponseFields[] = {"ok", "contract"};
static constexpr const char *kStorageSemanticsResponseFields[] = {"ok", "apiVersion", "objects", "backendCapabilities"};
static constexpr const char *kHealthRequired[] = {"ok", "wifiConnected", "ip", "uptimeMs", "timeAuthority", "timeSource", "timeConfidence", "filesystemReady", "assetContractReady"};
static constexpr const char *kHealthSections[] = {"healthStatus"};
static constexpr const char *kMetaRequired[] = {
    "ok",
    "apiVersion",
    "stateVersion",
    "storageSchemaVersion",
    "timeAuthority",
    "timeSource",
    "timeConfidence",
};
static constexpr const char *kMetaSections[] = {
    "versions",
    "storageRuntime",
    "assetContract",
    "runtimeEvents",
    "platformSupport",
    "deviceIdentity",
    "capabilities",
    "stateSurfaces",
    "releaseValidation",
};
static constexpr const char *kDisplayFrameRequired[] = {"ok", "width", "height", "presentCount", "bufferHex"};
static constexpr const char *kDisplayFrameSections[] = {"displayFrame"};
static constexpr const char *kTrackedActionAcceptedRequired[] = {"ok", "actionId", "requestId", "actionType", "trackPath", "queueDepth"};
static constexpr const char *kTrackedActionAcceptedSections[] = {"trackedActionAccepted"};
static constexpr const char *kActionsStatusRequired[] = {"ok", "id", "status", "type"};
static constexpr const char *kActionsStatusSections[] = {"actionStatus"};
static constexpr const char *kActionsCatalogRequired[] = {"ok", "commands"};
static constexpr const char *kActionsCatalogSections[] = {"commandCatalog"};
static constexpr const char *kResetActionRequired[] = {"ok", "message", "resetKind", "runtimeReload", "runtimeReload.runtimeReloadRequested", "runtimeReload.runtimeReloadPreflightOk", "runtimeReload.runtimeReloadApplyAttempted", "runtimeReload.runtimeReloaded", "runtimeReload.runtimeReloadEventDispatchOk", "runtimeReload.runtimeReloadAuthoritativePath", "runtimeReload.runtimeReloadVerifyOk", "runtimeReload.runtimeReloadPartialSuccess", "runtimeReload.runtimeHandlerCount", "runtimeReload.runtimeMatchedHandlerCount", "runtimeReload.runtimeHandlerSuccessCount", "runtimeReload.runtimeHandlerFailureCount", "runtimeReload.runtimeHandlerCriticalFailureCount", "runtimeReload.runtimeReloadConfigGeneration", "runtimeReload.runtimeReloadDomainResultCount", "runtimeReload.runtimeReloadSupportedDomains", "runtimeReload.runtimeReloadImpactDomains", "runtimeReload.runtimeReloadAppliedDomains", "runtimeReload.runtimeReloadFailedDomains", "runtimeReload.runtimeReloadDomainResults", "runtimeReload.runtimeReloadFailurePhase", "runtimeReload.runtimeReloadFailureCode"};
static constexpr const char *kResetActionSections[] = {"resetAction", "runtimeReload"};
static constexpr const char *kDeviceConfigUpdateRequired[] = {"ok", "runtimeReload", "runtimeReload.runtimeReloadRequested", "runtimeReload.runtimeReloadPreflightOk", "runtimeReload.runtimeReloadApplyAttempted", "runtimeReload.runtimeReloaded", "runtimeReload.runtimeReloadEventDispatchOk", "runtimeReload.runtimeReloadAuthoritativePath", "runtimeReload.runtimeReloadVerifyOk", "runtimeReload.runtimeReloadPartialSuccess", "runtimeReload.runtimeHandlerCount", "runtimeReload.runtimeMatchedHandlerCount", "runtimeReload.runtimeHandlerSuccessCount", "runtimeReload.runtimeHandlerFailureCount", "runtimeReload.runtimeHandlerCriticalFailureCount", "runtimeReload.runtimeReloadConfigGeneration", "runtimeReload.runtimeReloadDomainResultCount", "runtimeReload.runtimeReloadSupportedDomains", "runtimeReload.runtimeReloadImpactDomains", "runtimeReload.runtimeReloadAppliedDomains", "runtimeReload.runtimeReloadFailedDomains", "runtimeReload.runtimeReloadDomainResults", "runtimeReload.runtimeReloadFailurePhase", "runtimeReload.runtimeReloadFailureCode"};
static constexpr const char *kDeviceConfigUpdateSections[] = {"deviceConfigUpdate", "runtimeReload"};
static constexpr const char *kConfigDeviceReadbackRequired[] = {"ok", "config"};
static constexpr const char *kConfigDeviceReadbackSections[] = {"deviceConfigReadback", "capabilities"};
static constexpr const char *kContractDocumentRequired[] = {"ok", "contract"};
static constexpr const char *kContractDocumentSections[] = {"contract"};
static constexpr const char *kStorageSemanticsRequired[] = {"ok", "apiVersion", "objects", "backendCapabilities"};
static constexpr const char *kStorageSemanticsSections[] = {"storageSemantics"};
static constexpr const char *kReleaseValidationRequired[] = {
    "candidateBundleKind",
    "verifiedBundleKind",
    "hostReportType",
    "deviceReportType",
    "validationSchemaVersion",
};
static constexpr const char *kReleaseValidationSections[] = {"releaseValidation"};

static constexpr ApiSchemaSpec kApiSchemas[] = {
    {"health", kHealthRequired, 9U, kHealthSections, 1U},
    {"meta", kMetaRequired, 7U, kMetaSections, 9U},
    {"displayFrame", kDisplayFrameRequired, 5U, kDisplayFrameSections, 1U},
    {"trackedActionAccepted", kTrackedActionAcceptedRequired, 6U, kTrackedActionAcceptedSections, 1U},
    {"actionsStatus", kActionsStatusRequired, 4U, kActionsStatusSections, 1U},
    {"actionsCatalog", kActionsCatalogRequired, 2U, kActionsCatalogSections, 1U},
    {"resetActionResponse", kResetActionRequired, sizeof(kResetActionRequired) / sizeof(kResetActionRequired[0]), kResetActionSections, 2U},
    {"deviceConfigUpdate", kDeviceConfigUpdateRequired, sizeof(kDeviceConfigUpdateRequired) / sizeof(kDeviceConfigUpdateRequired[0]), kDeviceConfigUpdateSections, 2U},
    {"configDeviceReadback", kConfigDeviceReadbackRequired, 2U, kConfigDeviceReadbackSections, 2U},
    {"contractDocument", kContractDocumentRequired, 2U, kContractDocumentSections, 1U},
    {"storageSemantics", kStorageSemanticsRequired, 4U, kStorageSemanticsSections, 1U},
    {"releaseValidation", kReleaseValidationRequired, 5U, kReleaseValidationSections, 1U},
};

static constexpr RouteSchemaSpec kRouteSchemas[] = {
    /* {"contract", "api", "contractDocument"} */
    {"contract", "api", "contractDocument", "tooling", "web_contract", "rich_console", "schema-versioned", kNoRouteFields, 0U, kContractResponseFields, sizeof(kContractResponseFields) / sizeof(kContractResponseFields[0])},
    /* {"health", "api", "health"} */
    {"health", "api", "health", "control", "web_server", "rich_console", "stable", kNoRouteFields, 0U, kHealthResponseFields, sizeof(kHealthResponseFields) / sizeof(kHealthResponseFields[0])},
    /* {"meta", "api", "meta"} */
    {"meta", "api", "meta", "control", "web_routes_api", "rich_console", "stable", kNoRouteFields, 0U, kMetaResponseFields, sizeof(kMetaResponseFields) / sizeof(kMetaResponseFields[0])},
    /* {"actionsCatalog", "api", "actionsCatalog"} */
    {"actionsCatalog", "api", "actionsCatalog", "control", "app_command", "rich_console", "stable", kNoRouteFields, 0U, kActionsCatalogResponseFields, sizeof(kActionsCatalogResponseFields) / sizeof(kActionsCatalogResponseFields[0])},
    /* {"actionsLatest", "api", "actionsStatus"} */
    {"actionsLatest", "api", "actionsStatus", "console", "web_action_queue", "rich_console", "stable", kNoRouteFields, 0U, kActionsStatusResponseFields, sizeof(kActionsStatusResponseFields) / sizeof(kActionsStatusResponseFields[0])},
    /* {"actionsStatus", "api", "actionsStatus"} */
    {"actionsStatus", "api", "actionsStatus", "control", "web_action_queue", "rich_console", "stable", kNoRouteFields, 0U, kActionsStatusResponseFields, sizeof(kActionsStatusResponseFields) / sizeof(kActionsStatusResponseFields[0])},
    /* {"stateSummary", "state", "summary"} */
    {"stateSummary", "state", "summary", "compatibility", "web_state_bridge", "legacy_clients", "replace-with-stateAggregate", kNoRouteFields, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
    /* {"stateDetail", "state", "detail"} */
    {"stateDetail", "state", "detail", "diagnostic", "web_state_bridge", "rich_console", "stable", kNoRouteFields, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
    /* {"statePerf", "state", "perf"} */
    {"statePerf", "state", "perf", "diagnostic", "web_state_bridge", "rich_console", "stable", kNoRouteFields, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
    /* {"stateRaw", "state", "raw"} */
    {"stateRaw", "state", "raw", "tooling", "web_state_bridge", "internal_tools", "no-external-dependents", kNoRouteFields, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
    /* {"stateAggregate", "state", "aggregate"} */
    {"stateAggregate", "state", "aggregate", "control", "web_state_bridge", "rich_console", "stable", kNoRouteFields, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
    /* {"displayFrame", "api", "displayFrame"} */
    {"displayFrame", "api", "displayFrame", "diagnostic", "display_service", "rich_console", "stable", kNoRouteFields, 0U, kDisplayFrameResponseFields, sizeof(kDisplayFrameResponseFields) / sizeof(kDisplayFrameResponseFields[0])},
    /* {"displayOverlay", "api", "trackedActionAccepted"} */
    {"displayOverlay", "api", "trackedActionAccepted", "control", "web_overlay", "rich_console", "stable", kNoRouteFields, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
    /* {"displayOverlayClear", "api", "trackedActionAccepted"} */
    {"displayOverlayClear", "api", "trackedActionAccepted", "control", "web_overlay", "rich_console", "stable", kNoRouteFields, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
    /* {"configDevice", "api", "configDeviceReadback"} */
    {"configDevice", "api", "configDeviceReadback", "control", "device_config_authority", "rich_console", "stable", kConfigDeviceRequestFields, sizeof(kConfigDeviceRequestFields) / sizeof(kConfigDeviceRequestFields[0]), kDeviceConfigUpdateRequired, sizeof(kDeviceConfigUpdateRequired) / sizeof(kDeviceConfigUpdateRequired[0])},
    /* {"inputKey", "api", "trackedActionAccepted"} */
    {"inputKey", "api", "trackedActionAccepted", "control", "key_input", "rich_console", "stable", kNoRouteFields, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
    /* {"command", "api", "trackedActionAccepted"} */
    {"command", "api", "trackedActionAccepted", "control", "app_command", "rich_console", "stable", kNoRouteFields, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
    /* {"resetAppState", "api", "resetActionResponse"} */
    {"resetAppState", "api", "resetActionResponse", "control", "reset_service", "rich_console", "stable", kNoRouteFields, 0U, kResetResponseFields, sizeof(kResetResponseFields) / sizeof(kResetResponseFields[0])},
    /* {"resetDeviceConfig", "api", "resetActionResponse"} */
    {"resetDeviceConfig", "api", "resetActionResponse", "control", "reset_service", "rich_console", "stable", kNoRouteFields, 0U, kResetResponseFields, sizeof(kResetResponseFields) / sizeof(kResetResponseFields[0])},
    /* {"resetFactory", "api", "resetActionResponse"} */
    {"resetFactory", "api", "resetActionResponse", "control", "reset_service", "rich_console", "stable", kNoRouteFields, 0U, kResetResponseFields, sizeof(kResetResponseFields) / sizeof(kResetResponseFields[0])},
    /* {"storageSemantics", "api", "storageSemantics"} */
    {"storageSemantics", "api", "storageSemantics", "diagnostic", "storage_semantics", "rich_console", "stable", kNoRouteFields, 0U, kStorageSemanticsResponseFields, sizeof(kStorageSemanticsResponseFields) / sizeof(kStorageSemanticsResponseFields[0])},
};


static void append_state_schemas(String &response)
{
    response += "\"stateSchemas\":{";
    response += "\"summary\":";
    append_state_schema_entries(response, kStateSchemaSummary, sizeof(kStateSchemaSummary) / sizeof(kStateSchemaSummary[0]));
    response += ',';
    response += "\"detail\":";
    append_state_schema_entries(response, kStateSchemaDetail, sizeof(kStateSchemaDetail) / sizeof(kStateSchemaDetail[0]));
    response += ',';
    response += "\"perf\":";
    append_state_schema_entries(response, kStateSchemaPerf, sizeof(kStateSchemaPerf) / sizeof(kStateSchemaPerf[0]));
    response += ',';
    response += "\"raw\":";
    append_state_schema_entries(response, kStateSchemaRaw, sizeof(kStateSchemaRaw) / sizeof(kStateSchemaRaw[0]));
    response += ',';
    response += "\"aggregate\":";
    append_state_schema_entries(response, kStateSchemaAggregate, sizeof(kStateSchemaAggregate) / sizeof(kStateSchemaAggregate[0]));
    response += '}';
}

static void append_release_validation_schema(String &response)
{
    response += "\"releaseValidation\":{";
    web_json_kv_str(response, "candidateBundleKind", "candidate", false);
    web_json_kv_str(response, "verifiedBundleKind", "verified", false);
    web_json_kv_str(response, "hostReportType", "HOST_VALIDATION_REPORT", false);
    web_json_kv_str(response, "deviceReportType", "DEVICE_SMOKE_REPORT", false);
    web_json_kv_u32(response, "validationSchemaVersion", WEB_RELEASE_VALIDATION_SCHEMA_VERSION, true);
    response += '}';
}

static void append_api_schemas(String &response)
{
    response += "\"apiSchemas\":{";
    for (size_t i = 0; i < sizeof(kApiSchemas) / sizeof(kApiSchemas[0]); ++i) {
        append_api_schema(response, kApiSchemas[i], i + 1U == sizeof(kApiSchemas) / sizeof(kApiSchemas[0]));
    }
    response += '}';
}

static void append_route_schemas(String &response)
{
    response += "\"routeSchemas\":{";
    for (size_t i = 0; i < sizeof(kRouteSchemas) / sizeof(kRouteSchemas[0]); ++i) {
        const bool trailing = i + 1U == sizeof(kRouteSchemas) / sizeof(kRouteSchemas[0]);
        response += '"';
        response += kRouteSchemas[i].route_key;
        response += "\":{";
        web_json_kv_str(response, "kind", kRouteSchemas[i].schema_kind, false);
        web_json_kv_str(response, "name", kRouteSchemas[i].schema_name, false);
        web_json_kv_str(response, "tier", kRouteSchemas[i].surface_tier, false);
        web_json_kv_str(response, "producerOwner", kRouteSchemas[i].producer_owner, false);
        web_json_kv_str(response, "consumerOwner", kRouteSchemas[i].consumer_owner, false);
        web_json_kv_str(response, "deprecationPolicy", kRouteSchemas[i].deprecation_policy, false);
        response += "\"requestFields\":";
        append_string_array(response, kRouteSchemas[i].request_fields, kRouteSchemas[i].request_field_count);
        response += ',';
        response += "\"responseFields\":";
        append_string_array(response, kRouteSchemas[i].response_fields, kRouteSchemas[i].response_field_count);
        response += '}';
        if (!trailing) {
            response += ',';
        }
    }
    response += '}';
}

} // namespace


bool web_contract_get_api_schema(const char *name, WebApiSchemaView *out)
{
    if (name == nullptr || out == nullptr) {
        return false;
    }
    for (size_t i = 0; i < sizeof(kApiSchemas) / sizeof(kApiSchemas[0]); ++i) {
        if (strcmp(kApiSchemas[i].name, name) == 0) {
            out->name = kApiSchemas[i].name;
            out->required = kApiSchemas[i].required;
            out->required_count = kApiSchemas[i].required_count;
            out->sections = kApiSchemas[i].sections;
            out->section_count = kApiSchemas[i].section_count;
            return true;
        }
    }
    return false;
}

bool web_contract_get_route_schema(const char *route_key, WebRouteSchemaView *out)
{
    if (route_key == nullptr || out == nullptr) {
        return false;
    }
    for (size_t i = 0; i < sizeof(kRouteSchemas) / sizeof(kRouteSchemas[0]); ++i) {
        if (strcmp(kRouteSchemas[i].route_key, route_key) == 0) {
            out->route_key = kRouteSchemas[i].route_key;
            out->schema_kind = kRouteSchemas[i].schema_kind;
            out->schema_name = kRouteSchemas[i].schema_name;
            out->surface_tier = kRouteSchemas[i].surface_tier;
            out->producer_owner = kRouteSchemas[i].producer_owner;
            out->consumer_owner = kRouteSchemas[i].consumer_owner;
            out->deprecation_policy = kRouteSchemas[i].deprecation_policy;
            out->request_fields = kRouteSchemas[i].request_fields;
            out->request_field_count = kRouteSchemas[i].request_field_count;
            out->response_fields = kRouteSchemas[i].response_fields;
            out->response_field_count = kRouteSchemas[i].response_field_count;
            return true;
        }
    }
    return false;
}

bool web_contract_get_route_api_schema(const char *route_key, WebApiSchemaView *out)
{
    WebRouteSchemaView route = {};
    if (!web_contract_get_route_schema(route_key, &route)) {
        return false;
    }
    if (strcmp(route.schema_kind, "api") != 0) {
        return false;
    }
    return web_contract_get_api_schema(route.schema_name, out);
}

void web_contract_append_json(String &response)
{
    response += "\"contract\":{";
    web_json_kv_u32(response, "runtimeContractVersion", WEB_RUNTIME_CONTRACT_VERSION, false);
    web_json_kv_u32(response, "apiVersion", WEB_API_VERSION, false);
    web_json_kv_u32(response, "stateVersion", WEB_STATE_VERSION, false);
    web_json_kv_u32(response, "commandCatalogVersion", WEB_COMMAND_CATALOG_VERSION, false);
    web_json_kv_u32(response, "assetContractVersion", WEB_ASSET_CONTRACT_VERSION, false);
    response += "\"routes\":{";
    append_route(response, "contract", WEB_ROUTE_CONTRACT, false);
    append_route(response, "health", WEB_ROUTE_HEALTH, false);
    append_route(response, "meta", WEB_ROUTE_META, false);
    append_route(response, "actionsCatalog", WEB_ROUTE_ACTIONS_CATALOG, false);
    append_route(response, "actionsLatest", WEB_ROUTE_ACTIONS_LATEST, false);
    append_route(response, "actionsStatus", WEB_ROUTE_ACTIONS_STATUS, false);
    append_route(response, "stateSummary", WEB_ROUTE_STATE_SUMMARY, false);
    append_route(response, "stateDetail", WEB_ROUTE_STATE_DETAIL, false);
    append_route(response, "statePerf", WEB_ROUTE_STATE_PERF, false);
    append_route(response, "stateRaw", WEB_ROUTE_STATE_RAW, false);
    append_route(response, "stateAggregate", WEB_ROUTE_STATE_AGGREGATE, false);
    append_route(response, "displayFrame", WEB_ROUTE_DISPLAY_FRAME, false);
    append_route(response, "displayOverlay", WEB_ROUTE_DISPLAY_OVERLAY, false);
    append_route(response, "displayOverlayClear", WEB_ROUTE_DISPLAY_OVERLAY_CLEAR, false);
    append_route(response, "configDevice", WEB_ROUTE_CONFIG_DEVICE, false);
    append_route(response, "inputKey", WEB_ROUTE_INPUT_KEY, false);
    append_route(response, "command", WEB_ROUTE_COMMAND, false);
    append_route(response, "resetAppState", WEB_ROUTE_RESET_APP_STATE, false);
    append_route(response, "resetDeviceConfig", WEB_ROUTE_RESET_DEVICE_CONFIG, false);
    append_route(response, "resetFactory", WEB_ROUTE_RESET_FACTORY, false);
    append_route(response, "storageSemantics", WEB_ROUTE_STORAGE_SEMANTICS, true);
    response += "},";
    append_route_schemas(response);
    response += ',';
    append_state_schemas(response);
    response += ',';
    append_release_validation_schema(response);
    response += ',';
    append_api_schemas(response);
    response += '}';
}
