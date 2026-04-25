#ifndef WEB_ROUTES_API_INTERNAL_H
#define WEB_ROUTES_API_INTERNAL_H

#include <ESPAsyncWebServer.h>

void web_send_health_response(AsyncWebServerRequest *request);
void web_send_contract_response(AsyncWebServerRequest *request);
void web_send_meta_response(AsyncWebServerRequest *request);
void web_send_storage_semantics_response(AsyncWebServerRequest *request);
void web_send_actions_catalog_response(AsyncWebServerRequest *request);
void web_send_latest_action_response(AsyncWebServerRequest *request);
void web_send_action_status_response(AsyncWebServerRequest *request);
void web_send_device_config_response(AsyncWebServerRequest *request);
void web_handle_config_device_post_common(AsyncWebServerRequest *request);
void web_send_state_detail_response(AsyncWebServerRequest *request);
void web_send_state_perf_response(AsyncWebServerRequest *request);
void web_send_state_raw_response(AsyncWebServerRequest *request);
void web_send_state_aggregate_response(AsyncWebServerRequest *request);
void web_send_sensor_response(AsyncWebServerRequest *request);
void web_send_display_frame_response(AsyncWebServerRequest *request);
void web_handle_input_key_common(AsyncWebServerRequest *request);
void web_handle_command_common(AsyncWebServerRequest *request);
void web_handle_display_overlay_common(AsyncWebServerRequest *request);
void web_handle_display_overlay_clear_common(AsyncWebServerRequest *request);
void web_handle_app_state_reset_common(AsyncWebServerRequest *request);
void web_handle_device_config_reset_common(AsyncWebServerRequest *request);
void web_handle_factory_reset_common(AsyncWebServerRequest *request);

#endif
