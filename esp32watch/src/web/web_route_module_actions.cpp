#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

void web_register_route_module_actions(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_ACTIONS_CATALOG, HTTP_GET, handle_actions_catalog_request);
    server.on(WEB_ROUTE_ACTIONS_LATEST, HTTP_GET, handle_actions_latest_request);
    server.on(WEB_ROUTE_ACTIONS_STATUS, HTTP_GET, handle_actions_status_request);
    server.on(WEB_ROUTE_COMMAND, HTTP_POST, handle_command_request, nullptr, capture_request_body);
}

void web_install_route_module_actions(void)
{
    (void)web_route_module_register("actions", web_register_route_module_actions);
}

