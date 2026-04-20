#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

void web_register_route_module_input(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_INPUT_KEY, HTTP_POST, handle_input_key_request, nullptr, capture_request_body);
}

void web_install_route_module_input(void)
{
    (void)web_route_module_register("input", web_register_route_module_input);
}

