#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

namespace {
static constexpr const char *kActionsCatalogResponseFields[] = {"ok", "commands"};
static constexpr const char *kActionsStatusResponseFields[] = {"ok", "id", "status", "type"};
static constexpr const char *kTrackedActionResponseFields[] = {"ok", "actionId", "requestId", "actionType", "trackPath", "queueDepth"};

static constexpr const char *kTrackedActionAcceptedRequired[] = {"ok", "actionId", "requestId", "actionType", "trackPath", "queueDepth"};
static constexpr const char *kTrackedActionAcceptedSections[] = {"trackedActionAccepted"};
static constexpr const char *kActionsStatusRequired[] = {"ok", "id", "status", "type"};
static constexpr const char *kActionsStatusSections[] = {"actionStatus"};
static constexpr const char *kActionsCatalogRequired[] = {"ok", "commands"};
static constexpr const char *kActionsCatalogSections[] = {"commandCatalog"};

static constexpr WebRouteOperationCatalogEntry kActionsCatalogOps[] = {
    {"GET", "api", "actionsCatalog", nullptr, 0U, kActionsCatalogResponseFields, sizeof(kActionsCatalogResponseFields) / sizeof(kActionsCatalogResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kActionsLatestOps[] = {
    {"GET", "api", "actionsStatus", nullptr, 0U, kActionsStatusResponseFields, sizeof(kActionsStatusResponseFields) / sizeof(kActionsStatusResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kActionsStatusOps[] = {
    {"GET", "api", "actionsStatus", nullptr, 0U, kActionsStatusResponseFields, sizeof(kActionsStatusResponseFields) / sizeof(kActionsStatusResponseFields[0])},
};
static constexpr WebRouteOperationCatalogEntry kCommandOps[] = {
    {"POST", "api", "trackedActionAccepted", nullptr, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
};

static constexpr WebRouteCatalogEntry kActionRoutes[] = {
    {"actionsCatalog", WEB_ROUTE_ACTIONS_CATALOG, "control", "app_command_catalog", "rich_console", "stable", "GET", kActionsCatalogOps, sizeof(kActionsCatalogOps) / sizeof(kActionsCatalogOps[0])},
    {"actionsLatest", WEB_ROUTE_ACTIONS_LATEST, "control", "web_action_queue", "rich_console", "stable", "GET", kActionsLatestOps, sizeof(kActionsLatestOps) / sizeof(kActionsLatestOps[0])},
    {"actionsStatus", WEB_ROUTE_ACTIONS_STATUS, "control", "web_action_queue", "rich_console", "stable", "GET", kActionsStatusOps, sizeof(kActionsStatusOps) / sizeof(kActionsStatusOps[0])},
    {"command", WEB_ROUTE_COMMAND, "control", "app_command", "rich_console", "stable", "POST", kCommandOps, sizeof(kCommandOps) / sizeof(kCommandOps[0])},
};

static constexpr WebApiSchemaCatalogEntry kActionApiSchemas[] = {
    {"trackedActionAccepted", kTrackedActionAcceptedRequired, sizeof(kTrackedActionAcceptedRequired) / sizeof(kTrackedActionAcceptedRequired[0]), kTrackedActionAcceptedSections, sizeof(kTrackedActionAcceptedSections) / sizeof(kTrackedActionAcceptedSections[0])},
    {"actionsStatus", kActionsStatusRequired, sizeof(kActionsStatusRequired) / sizeof(kActionsStatusRequired[0]), kActionsStatusSections, sizeof(kActionsStatusSections) / sizeof(kActionsStatusSections[0])},
    {"actionsCatalog", kActionsCatalogRequired, sizeof(kActionsCatalogRequired) / sizeof(kActionsCatalogRequired[0]), kActionsCatalogSections, sizeof(kActionsCatalogSections) / sizeof(kActionsCatalogSections[0])},
};
}

void web_register_route_module_actions(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_ACTIONS_CATALOG, HTTP_GET, handle_actions_catalog_request);
    server.on(WEB_ROUTE_ACTIONS_LATEST, HTTP_GET, handle_actions_latest_request);
    server.on(WEB_ROUTE_ACTIONS_STATUS, HTTP_GET, handle_actions_status_request);
    server.on(WEB_ROUTE_COMMAND, HTTP_POST, handle_command_request, nullptr, capture_request_body);
}

void web_install_route_module_actions(void)
{
    (void)web_route_module_register("actions", web_register_route_module_actions,
                                    kActionRoutes, sizeof(kActionRoutes) / sizeof(kActionRoutes[0]),
                                    kActionApiSchemas, sizeof(kActionApiSchemas) / sizeof(kActionApiSchemas[0]),
                                    nullptr, 0U);
}
