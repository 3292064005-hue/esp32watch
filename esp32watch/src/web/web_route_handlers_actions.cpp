#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"

void handle_actions_catalog_request(AsyncWebServerRequest *request) { web_send_actions_catalog_response(request); }
void handle_actions_latest_request(AsyncWebServerRequest *request) { web_send_latest_action_response(request); }
void handle_actions_status_request(AsyncWebServerRequest *request) { web_send_action_status_response(request); }
void handle_command_request(AsyncWebServerRequest *request) { web_handle_command_common(request); }
