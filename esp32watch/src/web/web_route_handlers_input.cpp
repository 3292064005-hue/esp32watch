#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"

void handle_input_key_request(AsyncWebServerRequest *request) { web_handle_input_key_common(request); }
