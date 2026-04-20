#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

void web_register_route_module_config(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_CONFIG_DEVICE, HTTP_GET, handle_config_device_get_request);
    server.on(WEB_ROUTE_CONFIG_DEVICE, HTTP_POST, handle_config_device_post_request, nullptr, capture_request_body);
}

void web_install_route_module_config(void)
{
    (void)web_route_module_register("config", web_register_route_module_config);
}

