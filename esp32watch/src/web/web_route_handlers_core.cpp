#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"

void handle_health_request(AsyncWebServerRequest *request) { web_send_health_response(request); }
void handle_contract_request(AsyncWebServerRequest *request) { web_send_contract_response(request); }
void handle_meta_request(AsyncWebServerRequest *request) { web_send_meta_response(request); }
void handle_storage_semantics_request(AsyncWebServerRequest *request) { web_send_storage_semantics_response(request); }
