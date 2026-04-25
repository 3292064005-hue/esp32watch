#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

namespace {
static constexpr const char *kStateRouteResponseFields[] = {"ok", "apiVersion", "stateVersion", "stateRevision"};
static constexpr const char *kFlagWifiRssi[] = {"WEB_STATE_SECTION_FLAG_WIFI_INCLUDE_RSSI"};
static constexpr const char *kFlagSystemAggregate[] = {
    "WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_APP_INIT_STAGE",
    "WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SLEEP_STATE",
    "WEB_STATE_SECTION_FLAG_SYSTEM_INCLUDE_SAFE_MODE",
};
static constexpr const char *kFlagDiagSafeMode[] = {"WEB_STATE_SECTION_FLAG_DIAG_INCLUDE_SAFE_MODE_FIELDS"};
static constexpr const char *kFlagWeatherUpdated[] = {"WEB_STATE_SECTION_FLAG_WEATHER_INCLUDE_UPDATED_AT"};
static constexpr const char *kFlagPerfHistory[] = {"WEB_STATE_SECTION_FLAG_PERF_INCLUDE_HISTORY"};
static constexpr const char *kFlagSensorRawAxes[] = {"WEB_STATE_SECTION_FLAG_SENSOR_INCLUDE_RAW_AXES"};

static constexpr WebRouteOperationCatalogEntry kStateDetailOps[] = {
    {"GET", "state", "detail", nullptr, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kStatePerfOps[] = {
    {"GET", "state", "perf", nullptr, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kStateRawOps[] = {
    {"GET", "state", "raw", nullptr, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kStateAggregateOps[] = {
    {"GET", "state", "aggregate", nullptr, 0U, kStateRouteResponseFields, sizeof(kStateRouteResponseFields) / sizeof(kStateRouteResponseFields[0])},
};
static constexpr WebRouteCatalogEntry kStateRoutes[] = {
    {"stateDetail", WEB_ROUTE_STATE_DETAIL, "diagnostic", "web_state_bridge", "rich_console", "stable", "GET", kStateDetailOps, sizeof(kStateDetailOps) / sizeof(kStateDetailOps[0])},
    {"statePerf", WEB_ROUTE_STATE_PERF, "diagnostic", "watch_app_runtime", "rich_console", "stable", "GET", kStatePerfOps, sizeof(kStatePerfOps) / sizeof(kStatePerfOps[0])},
    {"stateRaw", WEB_ROUTE_STATE_RAW, "internal", "system_startup_report", "tooling", "stable", "GET", kStateRawOps, sizeof(kStateRawOps) / sizeof(kStateRawOps[0])},
    {"stateAggregate", WEB_ROUTE_STATE_AGGREGATE, "control", "web_state_bridge", "rich_console", "stable", "GET", kStateAggregateOps, sizeof(kStateAggregateOps) / sizeof(kStateAggregateOps[0])},
};

static constexpr WebStateSectionCatalogEntry kStateSchemaDetail[] = {
    {"activity", nullptr, 0U},
    {"alarm", nullptr, 0U},
    {"music", nullptr, 0U},
    {"sensor", nullptr, 0U},
    {"storage", nullptr, 0U},
    {"diag", kFlagDiagSafeMode, sizeof(kFlagDiagSafeMode) / sizeof(kFlagDiagSafeMode[0])},
    {"display", nullptr, 0U},
    {"weather", kFlagWeatherUpdated, sizeof(kFlagWeatherUpdated) / sizeof(kFlagWeatherUpdated[0])},
    {"terminal", nullptr, 0U},
    {"overlay", nullptr, 0U},
    {"perf", kFlagPerfHistory, sizeof(kFlagPerfHistory) / sizeof(kFlagPerfHistory[0])},
};
static constexpr WebStateSectionCatalogEntry kStateSchemaPerf[] = {
    {"perf", kFlagPerfHistory, sizeof(kFlagPerfHistory) / sizeof(kFlagPerfHistory[0])},
};
static constexpr WebStateSectionCatalogEntry kStateSchemaRaw[] = {
    {"startupRaw", nullptr, 0U},
    {"queueRaw", nullptr, 0U},
    {"sensor", kFlagSensorRawAxes, sizeof(kFlagSensorRawAxes) / sizeof(kFlagSensorRawAxes[0])},
    {"storage", nullptr, 0U},
    {"display", nullptr, 0U},
    {"perf", kFlagPerfHistory, sizeof(kFlagPerfHistory) / sizeof(kFlagPerfHistory[0])},
};
static constexpr WebStateSectionCatalogEntry kStateSchemaAggregate[] = {
    {"wifi", kFlagWifiRssi, sizeof(kFlagWifiRssi) / sizeof(kFlagWifiRssi[0])},
    {"system", kFlagSystemAggregate, sizeof(kFlagSystemAggregate) / sizeof(kFlagSystemAggregate[0])},
    {"config", nullptr, 0U},
    {"activity", nullptr, 0U},
    {"alarm", nullptr, 0U},
    {"music", nullptr, 0U},
    {"sensor", nullptr, 0U},
    {"storage", nullptr, 0U},
    {"diag", nullptr, 0U},
    {"display", nullptr, 0U},
    {"weather", kFlagWeatherUpdated, sizeof(kFlagWeatherUpdated) / sizeof(kFlagWeatherUpdated[0])},
    {"summary", nullptr, 0U},
    {"terminal", nullptr, 0U},
    {"overlay", nullptr, 0U},
    {"perf", nullptr, 0U},
};
static constexpr WebStateSchemaCatalogEntry kStateSchemas[] = {
    {"detail", kStateSchemaDetail, sizeof(kStateSchemaDetail) / sizeof(kStateSchemaDetail[0])},
    {"perf", kStateSchemaPerf, sizeof(kStateSchemaPerf) / sizeof(kStateSchemaPerf[0])},
    {"raw", kStateSchemaRaw, sizeof(kStateSchemaRaw) / sizeof(kStateSchemaRaw[0])},
    {"aggregate", kStateSchemaAggregate, sizeof(kStateSchemaAggregate) / sizeof(kStateSchemaAggregate[0])},
};
}

void web_register_route_module_state(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_STATE_DETAIL, HTTP_GET, handle_state_detail_request);
    server.on(WEB_ROUTE_STATE_PERF, HTTP_GET, handle_state_perf_request);
    server.on(WEB_ROUTE_STATE_RAW, HTTP_GET, handle_state_raw_request);
    server.on(WEB_ROUTE_STATE_AGGREGATE, HTTP_GET, handle_state_aggregate_request);
}

void web_install_route_module_state(void)
{
    (void)web_route_module_register("state", web_register_route_module_state,
                                    kStateRoutes, sizeof(kStateRoutes) / sizeof(kStateRoutes[0]),
                                    nullptr, 0U,
                                    kStateSchemas, sizeof(kStateSchemas) / sizeof(kStateSchemas[0]));
}
