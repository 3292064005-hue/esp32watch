#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"

void handle_config_device_get_request(AsyncWebServerRequest *request) { web_send_device_config_response(request); }
void handle_config_device_post_request(AsyncWebServerRequest *request) { web_handle_config_device_post_common(request); }
