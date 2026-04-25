#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

namespace {
static constexpr const char *kConfigDevicePostRequestFields[] = {"ssid", "password", "timezonePosix", "timezoneId", "latitude", "longitude", "locationName", "apiToken", "runtimeReloadDomains"};
static constexpr const char *kConfigDeviceGetResponseFields[] = {"ok", "config"};
static constexpr const char *kConfigDevicePostResponseFields[] = {"ok", "wifiSaved", "networkSaved", "tokenSaved", "runtimeReload"};

static constexpr const char *kDeviceConfigUpdateRequired[] = {"ok", "runtimeReload", "runtimeReload.runtimeReloadRequested", "runtimeReload.runtimeReloadPreflightOk", "runtimeReload.runtimeReloadApplyAttempted", "runtimeReload.runtimeReloaded", "runtimeReload.runtimeReloadEventDispatchOk", "runtimeReload.runtimeReloadAuthoritativePath", "runtimeReload.runtimeReloadVerifyOk", "runtimeReload.runtimeReloadPartialSuccess", "runtimeReload.runtimeHandlerCount", "runtimeReload.runtimeMatchedHandlerCount", "runtimeReload.runtimeHandlerSuccessCount", "runtimeReload.runtimeHandlerFailureCount", "runtimeReload.runtimeHandlerCriticalFailureCount", "runtimeReload.runtimeReloadConfigGeneration", "runtimeReload.runtimeReloadDomainResultCount", "runtimeReload.runtimeReloadSupportedDomains", "runtimeReload.runtimeReloadImpactDomains", "runtimeReload.runtimeReloadAppliedDomains", "runtimeReload.runtimeReloadFailedDomains", "runtimeReload.runtimeReloadDomainResults", "runtimeReload.runtimeReloadFailurePhase", "runtimeReload.runtimeReloadFailureCode"};
static constexpr const char *kDeviceConfigUpdateSections[] = {"deviceConfigUpdate", "runtimeReload"};
static constexpr const char *kConfigDeviceReadbackRequired[] = {"ok", "config"};
static constexpr const char *kConfigDeviceReadbackSections[] = {"deviceConfigReadback", "capabilities"};

static constexpr WebRouteOperationCatalogEntry kConfigDeviceOps[] = {
    {"GET", "api", "configDeviceReadback", nullptr, 0U, kConfigDeviceGetResponseFields, sizeof(kConfigDeviceGetResponseFields) / sizeof(kConfigDeviceGetResponseFields[0])},
    {"POST", "api", "deviceConfigUpdate", kConfigDevicePostRequestFields, sizeof(kConfigDevicePostRequestFields) / sizeof(kConfigDevicePostRequestFields[0]), kConfigDevicePostResponseFields, sizeof(kConfigDevicePostResponseFields) / sizeof(kConfigDevicePostResponseFields[0])},
};
static constexpr WebRouteCatalogEntry kConfigRoutes[] = {
    {"configDevice", WEB_ROUTE_CONFIG_DEVICE, "control", "device_config_authority", "rich_console", "stable", "GET", kConfigDeviceOps, sizeof(kConfigDeviceOps) / sizeof(kConfigDeviceOps[0])},
};
static constexpr WebApiSchemaCatalogEntry kConfigApiSchemas[] = {
    {"deviceConfigUpdate", kDeviceConfigUpdateRequired, sizeof(kDeviceConfigUpdateRequired) / sizeof(kDeviceConfigUpdateRequired[0]), kDeviceConfigUpdateSections, sizeof(kDeviceConfigUpdateSections) / sizeof(kDeviceConfigUpdateSections[0])},
    {"configDeviceReadback", kConfigDeviceReadbackRequired, sizeof(kConfigDeviceReadbackRequired) / sizeof(kConfigDeviceReadbackRequired[0]), kConfigDeviceReadbackSections, sizeof(kConfigDeviceReadbackSections) / sizeof(kConfigDeviceReadbackSections[0])},
};
}

void web_register_route_module_config(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_CONFIG_DEVICE, HTTP_GET, handle_config_device_get_request);
    server.on(WEB_ROUTE_CONFIG_DEVICE, HTTP_POST, handle_config_device_post_request, nullptr, capture_request_body);
}

void web_install_route_module_config(void)
{
    (void)web_route_module_register("config", web_register_route_module_config,
                                    kConfigRoutes, sizeof(kConfigRoutes) / sizeof(kConfigRoutes[0]),
                                    kConfigApiSchemas, sizeof(kConfigApiSchemas) / sizeof(kConfigApiSchemas[0]),
                                    nullptr, 0U);
}
