#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "web/web_action_queue.h"
#include "web/web_state_bridge.h"
#include "web/web_state_bridge_internal.h"
#include "web/web_json.h"
#include "web/web_wifi.h"
#include "web/web_server.h"
#include "web/web_api_config.h"
#include "web/web_runtime_snapshot.h"
#include "web/web_contract.h"
#include "web/web_contract_json.h"
#include "web/web_routes_api_handlers.h"
#include "web/web_routes_api_internal.h"
#include "web/web_route_module_registry.h"
#include "web/web_routes_api_support.h"
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cmath>

extern "C" {
#include "display_internal.h"
#include "key.h"
#include "app_command.h"
#include "main.h"
#include "services/device_config.h"
#include "services/network_sync_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "app_tuning.h"
#include "board_manifest.h"
#include "system_init.h"
#include "services/runtime_event_service.h"
#include "services/runtime_reload_coordinator.h"
#include "services/reset_service.h"
#include "services/storage_semantics.h"
#include "services/storage_facade.h"
#include "services/device_config_authority.h"
#include "services/time_service.h"
}

void web_send_sensor_response(AsyncWebServerRequest *request)
{
    const SensorSnapshot *snap = sensor_service_get_snapshot();
    String response = "{";
    if (snap == nullptr) {
        request->send(500, "application/json", "{\"ok\":false,\"message\":\"sensor unavailable\"}");
        return;
    }
    response += "\"ok\":true,";
    response += "\"online\":";
    response += snap->online ? "true" : "false";
    response += ",";
    web_json_kv_str(response, "runtimeState", sensor_runtime_state_name(snap->runtime_state), false);
    web_json_kv_str(response, "calibrationState", sensor_calibration_state_name(snap->calibration_state), false);
    web_json_kv_bool(response, "calibrated", snap->calibrated, false);
    web_json_kv_bool(response, "staticNow", snap->static_now, false);
    web_json_kv_u32(response, "quality", snap->quality, false);
    web_json_kv_u32(response, "errorCode", snap->error_code, false);
    web_json_kv_u32(response, "faultCount", snap->fault_count, false);
    web_json_kv_u32(response, "reinitCount", snap->reinit_count, false);
    web_json_kv_u32(response, "calibrationProgress", snap->calibration_progress, false);
    web_json_kv_i32(response, "ax", snap->ax, false);
    web_json_kv_i32(response, "ay", snap->ay, false);
    web_json_kv_i32(response, "az", snap->az, false);
    web_json_kv_i32(response, "gx", snap->gx, false);
    web_json_kv_i32(response, "gy", snap->gy, false);
    web_json_kv_i32(response, "gz", snap->gz, false);
    web_json_kv_i32(response, "accelNormMg", snap->accel_norm_mg, false);
    web_json_kv_i32(response, "baselineMg", snap->baseline_mg, false);
    web_json_kv_i32(response, "motionScore", snap->motion_score, false);
    web_json_kv_i32(response, "pitchDeg", snap->pitch_deg, false);
    web_json_kv_i32(response, "rollDeg", snap->roll_deg, false);
    web_json_kv_u32(response, "stepsTotal", snap->steps_total, false);
    web_json_kv_u32(response, "lastSampleMs", snap->last_sample_ms, true);
    response += "}";
    request->send(200, "application/json", response);
}

void web_send_display_frame_response(AsyncWebServerRequest *request)
{
    String response;
    response.reserve(128U + (OLED_BUFFER_SIZE * 4U));
    response += "{\"ok\":true,\"width\":";
    response += OLED_WIDTH;
    response += ",\"height\":";
    response += OLED_HEIGHT;
    response += ",\"presentCount\":";
    response += display_get_present_count();
    response += ",\"bufferHex\":\"";
    for (size_t i = 0; i < OLED_BUFFER_SIZE; ++i) {
        append_hex_byte(response, g_oled_buffer[i]);
    }
    response += "\",";
    append_display_frame_schema_sections(response);
    response += "}";
    request->send(200, "application/json", response);
}

void web_handle_input_key_common(AsyncWebServerRequest *request)
{
    StaticJsonDocument<256> json_doc;
    String key_str;
    String event_str;
    KeyId key_id = KEY_ID_UP;
    KeyEventType event_type = KEY_EVENT_SHORT;
    bool parsed = false;
    if (!require_control_auth(request)) {
        return;
    }
    if (read_form_field(request, "key", key_str)) {
        parsed = parse_key_id(key_str.c_str(), &key_id);
        if (read_form_field(request, "event", event_str) && event_str.length() > 0U) {
            if (!parse_key_event(event_str.c_str(), &event_type)) {
                send_json_error(request, 400, "invalid key event");
                return;
            }
        }
    } else if (parse_json_body(request, json_doc)) {
        if (!read_json_string_field(json_doc, "key", key_str)) {
            send_json_error(request, 400, "missing key");
            return;
        }
        parsed = parse_key_id(key_str.c_str(), &key_id);
        if (read_json_string_field(json_doc, "event", event_str) && event_str.length() > 0U) {
            if (!parse_key_event(event_str.c_str(), &event_type)) {
                send_json_error(request, 400, "invalid key event");
                return;
            }
        }
    } else {
        send_json_error(request, 400, "missing key payload");
        return;
    }
    if (!parsed) {
        send_json_error(request, 400, "invalid key");
        return;
    }
    WebAction action = {};
    action.type = WEB_ACTION_KEY;
    action.data.key.id = key_id;
    action.data.key.event_type = event_type;
    uint32_t action_id = 0U;
    if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
        send_json_error(request, 503, "action queue full");
        return;
    }
    send_queue_ack(request, action_id, action.type);
}

void web_handle_command_common(AsyncWebServerRequest *request)
{
    StaticJsonDocument<256> json_doc;
    String cmd_str;
    String error_message;
    AppCommandType cmd_type = (AppCommandType)0;
    const StaticJsonDocument<256> *json_payload = nullptr;
    if (!require_control_auth(request)) {
        return;
    }
    if (read_form_field(request, "type", cmd_str)) {
        if (!parse_command_type(cmd_str.c_str(), &cmd_type) || cmd_type == 0) {
            send_json_error(request, 400, "invalid command");
            return;
        }
    } else if (parse_json_body(request, json_doc)) {
        json_payload = &json_doc;
        if (!read_json_string_field(json_doc, "type", cmd_str)) {
            send_json_error(request, 400, "missing command type");
            return;
        }
        if (!parse_command_type(cmd_str.c_str(), &cmd_type) || cmd_type == 0) {
            send_json_error(request, 400, "invalid command");
            return;
        }
    } else {
        send_json_error(request, 400, "missing command payload");
        return;
    }
    WebAction action = {};
    action.type = WEB_ACTION_COMMAND;
    action.data.command.source = APP_COMMAND_SOURCE_UI;
    action.data.command.type = cmd_type;
    action.data.command.data.u32 = 0;
    if (!populate_command_payload_from_request(request, json_payload, &action.data.command, cmd_str, &error_message)) {
        send_json_error(request, 400, error_message.c_str());
        return;
    }
    uint32_t action_id = 0U;
    if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
        send_json_error(request, 503, "action queue full");
        return;
    }
    send_queue_ack(request, action_id, action.type);
}

void web_handle_display_overlay_common(AsyncWebServerRequest *request)
{
    StaticJsonDocument<256> json_doc;
    String overlay_text;
    String duration_str;
    uint32_t duration_ms = WEB_API_DEFAULT_OVERLAY_DURATION_MS;
    if (!require_control_auth(request)) {
        return;
    }
    if (read_form_field(request, "text", overlay_text)) {
        if (read_form_field(request, "durationMs", duration_str) && duration_str.length() > 0U) {
            if (!parse_strict_u32_field(duration_str, &duration_ms)) {
                send_json_error(request, 400, "invalid overlay duration");
                return;
            }
        }
    } else if (parse_json_body(request, json_doc)) {
        if (!read_json_string_field(json_doc, "text", overlay_text)) {
            send_json_error(request, 400, "missing overlay text");
            return;
        }
        if (!json_doc["durationMs"].isNull() && !read_json_u32_field(json_doc, "durationMs", &duration_ms)) {
            send_json_error(request, 400, "invalid overlay duration");
            return;
        }
    } else {
        send_json_error(request, 400, "missing overlay payload");
        return;
    }
    if (overlay_text.length() == 0U || overlay_text.length() > WEB_API_OVERLAY_MAX_TEXT_LEN || duration_ms == 0U || duration_ms > WEB_API_MAX_OVERLAY_DURATION_MS) {
        send_json_error(request, 400, "invalid overlay params");
        return;
    }
    WebAction action = {};
    action.type = WEB_ACTION_OVERLAY_TEXT;
    strncpy(action.data.overlay.text, overlay_text.c_str(), sizeof(action.data.overlay.text) - 1U);
    action.data.overlay.text[sizeof(action.data.overlay.text) - 1U] = '\0';
    action.data.overlay.duration_ms = duration_ms;
    uint32_t action_id = 0U;
    if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
        send_json_error(request, 503, "action queue full");
        return;
    }
    send_queue_ack(request, action_id, action.type);
}

void web_handle_display_overlay_clear_common(AsyncWebServerRequest *request)
{
    WebAction action = {};
    if (!require_control_auth(request)) {
        return;
    }
    action.type = WEB_ACTION_OVERLAY_CLEAR;
    uint32_t action_id = 0U;
    if (!web_action_enqueue_tracked(&action, platform_time_now_ms(), &action_id)) {
        send_json_error(request, 503, "action queue full");
        return;
    }
    send_queue_ack(request, action_id, action.type);
}

