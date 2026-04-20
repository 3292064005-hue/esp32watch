#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

void web_register_route_module_state(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_STATE_SUMMARY, HTTP_GET, handle_state_summary_request);
    server.on(WEB_ROUTE_STATE_DETAIL, HTTP_GET, handle_state_detail_request);
    server.on(WEB_ROUTE_STATE_PERF, HTTP_GET, handle_state_perf_request);
    server.on(WEB_ROUTE_STATE_RAW, HTTP_GET, handle_state_raw_request);
    server.on(WEB_ROUTE_STATE_AGGREGATE, HTTP_GET, handle_state_aggregate_request);
}

void web_install_route_module_state(void)
{
    (void)web_route_module_register("state", web_register_route_module_state);
}

