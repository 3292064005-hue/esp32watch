#include <cstdio>
#include "Arduino.h"
#include "web/web_routes_api_handlers.h"
#include "web/web_contract_json.h"

void handle_health_request(AsyncWebServerRequest *request) { (void)request; }
void handle_contract_request(AsyncWebServerRequest *request) { (void)request; }
void handle_meta_request(AsyncWebServerRequest *request) { (void)request; }
void handle_storage_semantics_request(AsyncWebServerRequest *request) { (void)request; }
void handle_actions_catalog_request(AsyncWebServerRequest *request) { (void)request; }
void handle_actions_latest_request(AsyncWebServerRequest *request) { (void)request; }
void handle_actions_status_request(AsyncWebServerRequest *request) { (void)request; }
void handle_config_device_get_request(AsyncWebServerRequest *request) { (void)request; }
void handle_config_device_post_request(AsyncWebServerRequest *request) { (void)request; }
void handle_state_detail_request(AsyncWebServerRequest *request) { (void)request; }
void handle_state_perf_request(AsyncWebServerRequest *request) { (void)request; }
void handle_state_raw_request(AsyncWebServerRequest *request) { (void)request; }
void handle_state_aggregate_request(AsyncWebServerRequest *request) { (void)request; }
void handle_sensor_request(AsyncWebServerRequest *request) { (void)request; }
void handle_display_frame_request(AsyncWebServerRequest *request) { (void)request; }
void handle_input_key_request(AsyncWebServerRequest *request) { (void)request; }
void handle_command_request(AsyncWebServerRequest *request) { (void)request; }
void handle_display_overlay_request(AsyncWebServerRequest *request) { (void)request; }
void handle_display_overlay_clear_request(AsyncWebServerRequest *request) { (void)request; }
void handle_reset_app_state_route_request(AsyncWebServerRequest *request) { (void)request; }
void handle_reset_device_config_route_request(AsyncWebServerRequest *request) { (void)request; }
void handle_reset_factory_route_request(AsyncWebServerRequest *request) { (void)request; }
void capture_request_body(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    (void)request;
    (void)data;
    (void)len;
    (void)index;
    (void)total;
}

int main() {
    String response("{");
    response += "\"ok\":true,";
    web_contract_append_json(response);
    response += "}";
    std::fputs(response.c_str(), stdout);
    return 0;
}
