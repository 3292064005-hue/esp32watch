#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

void web_register_route_module_reset(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_RESET_APP_STATE, HTTP_POST, handle_reset_app_state_route_request, nullptr, capture_request_body);
    server.on(WEB_ROUTE_RESET_DEVICE_CONFIG, HTTP_POST, handle_reset_device_config_route_request, nullptr, capture_request_body);
    server.on(WEB_ROUTE_RESET_FACTORY, HTTP_POST, handle_reset_factory_route_request, nullptr, capture_request_body);
}

void web_install_route_module_reset(void)
{
    (void)web_route_module_register("reset", web_register_route_module_reset);
}

