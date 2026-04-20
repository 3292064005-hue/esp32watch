#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"

void handle_state_summary_request(AsyncWebServerRequest *request) { web_send_state_summary_response(request); }
void handle_state_detail_request(AsyncWebServerRequest *request) { web_send_state_detail_response(request); }
void handle_state_perf_request(AsyncWebServerRequest *request) { web_send_state_perf_response(request); }
void handle_state_raw_request(AsyncWebServerRequest *request) { web_send_state_raw_response(request); }
void handle_state_aggregate_request(AsyncWebServerRequest *request) { web_send_state_aggregate_response(request); }
