#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

namespace {
void handle_input_key_request_releasing_body(AsyncWebServerRequest *request)
{
    handle_input_key_request(request);
    release_request_body(request);
}

static constexpr const char *kTrackedActionResponseFields[] = {"ok", "actionId", "requestId", "actionType", "trackPath", "queueDepth"};
static constexpr WebRouteOperationCatalogEntry kInputKeyOps[] = {
    {"POST", "api", "trackedActionAccepted", nullptr, 0U, kTrackedActionResponseFields, sizeof(kTrackedActionResponseFields) / sizeof(kTrackedActionResponseFields[0])},
};
static constexpr WebRouteCatalogEntry kInputRoutes[] = {
    {"inputKey", WEB_ROUTE_INPUT_KEY, "control", "key_input", "rich_console", "stable", "POST", kInputKeyOps, sizeof(kInputKeyOps) / sizeof(kInputKeyOps[0])},
};
}

void web_register_route_module_input(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_INPUT_KEY, HTTP_POST, handle_input_key_request_releasing_body, nullptr, capture_request_body);
}

void web_install_route_module_input(void)
{
    (void)web_route_module_register("input", web_register_route_module_input,
                                    kInputRoutes, sizeof(kInputRoutes) / sizeof(kInputRoutes[0]),
                                    nullptr, 0U,
                                    nullptr, 0U);
}
