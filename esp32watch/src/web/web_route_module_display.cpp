#include "web/web_route_module_registry.h"
#include "web/web_contract.h"
#include "web/web_routes_api_handlers.h"

void web_register_route_module_display(AsyncWebServer &server)
{
    server.on(WEB_ROUTE_DISPLAY_FRAME, HTTP_GET, handle_display_frame_request);
    server.on(WEB_ROUTE_DISPLAY_OVERLAY, HTTP_POST, handle_display_overlay_request, nullptr, capture_request_body);
    server.on(WEB_ROUTE_DISPLAY_OVERLAY_CLEAR, HTTP_POST, handle_display_overlay_clear_request, nullptr, capture_request_body);
}

void web_install_route_module_display(void)
{
    (void)web_route_module_register("display", web_register_route_module_display);
}

