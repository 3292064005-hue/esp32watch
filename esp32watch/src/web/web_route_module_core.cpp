#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

void web_register_route_module_core(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_HEALTH, HTTP_GET, handle_health_request);
    server.on(WEB_ROUTE_CONTRACT, HTTP_GET, handle_contract_request);
    server.on(WEB_ROUTE_META, HTTP_GET, handle_meta_request);
    server.on(WEB_ROUTE_STORAGE_SEMANTICS, HTTP_GET, handle_storage_semantics_request);
}

void web_install_route_module_core(void)
{
    (void)web_route_module_register("core", web_register_route_module_core);
}

