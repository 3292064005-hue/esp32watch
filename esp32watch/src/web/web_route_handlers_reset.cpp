#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"

void handle_reset_app_state_route_request(AsyncWebServerRequest *request) { web_handle_app_state_reset_common(request); }
void handle_reset_device_config_route_request(AsyncWebServerRequest *request) { web_handle_device_config_reset_common(request); }
void handle_reset_factory_route_request(AsyncWebServerRequest *request) { web_handle_factory_reset_common(request); }
