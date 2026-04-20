#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"

void handle_sensor_request(AsyncWebServerRequest *request) { web_send_sensor_response(request); }
void handle_display_frame_request(AsyncWebServerRequest *request) { web_send_display_frame_response(request); }
void handle_display_overlay_request(AsyncWebServerRequest *request) { web_handle_display_overlay_common(request); }
void handle_display_overlay_clear_request(AsyncWebServerRequest *request) { web_handle_display_overlay_clear_common(request); }
