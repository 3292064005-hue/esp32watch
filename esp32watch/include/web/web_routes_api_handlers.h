#ifndef WEB_ROUTES_API_HANDLERS_H
#define WEB_ROUTES_API_HANDLERS_H

#include <ESPAsyncWebServer.h>

void handle_health_request(AsyncWebServerRequest *request);
void handle_contract_request(AsyncWebServerRequest *request);
void handle_meta_request(AsyncWebServerRequest *request);
void handle_storage_semantics_request(AsyncWebServerRequest *request);
void handle_actions_catalog_request(AsyncWebServerRequest *request);
void handle_actions_latest_request(AsyncWebServerRequest *request);
void handle_actions_status_request(AsyncWebServerRequest *request);
void handle_config_device_get_request(AsyncWebServerRequest *request);
void handle_config_device_post_request(AsyncWebServerRequest *request);
void handle_state_detail_request(AsyncWebServerRequest *request);
void handle_state_perf_request(AsyncWebServerRequest *request);
void handle_state_raw_request(AsyncWebServerRequest *request);
void handle_state_aggregate_request(AsyncWebServerRequest *request);
void handle_sensor_request(AsyncWebServerRequest *request);
void handle_display_frame_request(AsyncWebServerRequest *request);
void handle_input_key_request(AsyncWebServerRequest *request);
void handle_command_request(AsyncWebServerRequest *request);
void handle_display_overlay_request(AsyncWebServerRequest *request);
void handle_display_overlay_clear_request(AsyncWebServerRequest *request);
void handle_reset_app_state_route_request(AsyncWebServerRequest *request);
void handle_reset_device_config_route_request(AsyncWebServerRequest *request);
void handle_reset_factory_route_request(AsyncWebServerRequest *request);
void capture_request_body(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void release_request_body(AsyncWebServerRequest *request);

#endif
