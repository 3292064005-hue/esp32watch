#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

namespace {
static constexpr const char *kResetResponseFields[] = {"ok", "message", "resetKind", "runtimeReload"};
static constexpr const char *kResetActionRequired[] = {"ok", "message", "resetKind", "runtimeReload", "runtimeReload.runtimeReloadRequested", "runtimeReload.runtimeReloadPreflightOk", "runtimeReload.runtimeReloadApplyAttempted", "runtimeReload.runtimeReloaded", "runtimeReload.runtimeReloadEventDispatchOk", "runtimeReload.runtimeReloadAuthoritativePath", "runtimeReload.runtimeReloadVerifyOk", "runtimeReload.runtimeReloadPartialSuccess", "runtimeReload.runtimeHandlerCount", "runtimeReload.runtimeMatchedHandlerCount", "runtimeReload.runtimeHandlerSuccessCount", "runtimeReload.runtimeHandlerFailureCount", "runtimeReload.runtimeHandlerCriticalFailureCount", "runtimeReload.runtimeReloadConfigGeneration", "runtimeReload.runtimeReloadDomainResultCount", "runtimeReload.runtimeReloadSupportedDomains", "runtimeReload.runtimeReloadImpactDomains", "runtimeReload.runtimeReloadAppliedDomains", "runtimeReload.runtimeReloadFailedDomains", "runtimeReload.runtimeReloadDomainResults", "runtimeReload.runtimeReloadFailurePhase", "runtimeReload.runtimeReloadFailureCode"};
static constexpr const char *kResetActionSections[] = {"resetAction", "runtimeReload"};

static constexpr WebRouteOperationCatalogEntry kResetAppStateOps[] = {
    {"POST", "api", "resetActionResponse", nullptr, 0U, kResetResponseFields, sizeof(kResetResponseFields) / sizeof(kResetResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kResetDeviceConfigOps[] = {
    {"POST", "api", "resetActionResponse", nullptr, 0U, kResetResponseFields, sizeof(kResetResponseFields) / sizeof(kResetResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kResetFactoryOps[] = {
    {"POST", "api", "resetActionResponse", nullptr, 0U, kResetResponseFields, sizeof(kResetResponseFields) / sizeof(kResetResponseFields[0])},
};
static constexpr WebRouteCatalogEntry kResetRoutes[] = {
    {"resetAppState", WEB_ROUTE_RESET_APP_STATE, "control", "reset_service", "rich_console", "stable", "POST", kResetAppStateOps, sizeof(kResetAppStateOps) / sizeof(kResetAppStateOps[0])},
    {"resetDeviceConfig", WEB_ROUTE_RESET_DEVICE_CONFIG, "control", "reset_service", "rich_console", "stable", "POST", kResetDeviceConfigOps, sizeof(kResetDeviceConfigOps) / sizeof(kResetDeviceConfigOps[0])},
    {"resetFactory", WEB_ROUTE_RESET_FACTORY, "control", "reset_service", "rich_console", "stable", "POST", kResetFactoryOps, sizeof(kResetFactoryOps) / sizeof(kResetFactoryOps[0])},
};
static constexpr WebApiSchemaCatalogEntry kResetApiSchemas[] = {
    {"resetActionResponse", kResetActionRequired, sizeof(kResetActionRequired) / sizeof(kResetActionRequired[0]), kResetActionSections, sizeof(kResetActionSections) / sizeof(kResetActionSections[0])},
};
}

void web_register_route_module_reset(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_RESET_APP_STATE, HTTP_POST, handle_reset_app_state_route_request, nullptr, capture_request_body);
    server.on(WEB_ROUTE_RESET_DEVICE_CONFIG, HTTP_POST, handle_reset_device_config_route_request, nullptr, capture_request_body);
    server.on(WEB_ROUTE_RESET_FACTORY, HTTP_POST, handle_reset_factory_route_request, nullptr, capture_request_body);
}

void web_install_route_module_reset(void)
{
    (void)web_route_module_register("reset", web_register_route_module_reset,
                                    kResetRoutes, sizeof(kResetRoutes) / sizeof(kResetRoutes[0]),
                                    kResetApiSchemas, sizeof(kResetApiSchemas) / sizeof(kResetApiSchemas[0]),
                                    nullptr, 0U);
}
