#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include "web/web_action_queue.h"
#include "web/web_state_bridge.h"
#include "web/web_json.h"
#include "web/web_wifi.h"
#include <cstdlib>
#include <cstring>

extern "C" {
#include "display_internal.h"
#include "key.h"
#include "app_command.h"
#include "main.h"
#include "services/sensor_service.h"
}

static const char *kHexDigits = "0123456789ABCDEF";

static bool parse_key_id(const char *key_str, KeyId *out_id)
{
    if (!key_str || !out_id) {
        return false;
    }

    if (strcmp(key_str, "up") == 0) {
        *out_id = KEY_ID_UP;
        return true;
    } else if (strcmp(key_str, "down") == 0) {
        *out_id = KEY_ID_DOWN;
        return true;
    } else if (strcmp(key_str, "ok") == 0) {
        *out_id = KEY_ID_OK;
        return true;
    } else if (strcmp(key_str, "back") == 0) {
        *out_id = KEY_ID_BACK;
        return true;
    }

    return false;
}

static bool parse_key_event(const char *event_str, KeyEventType *out_event)
{
    if (!event_str || !out_event) {
        return false;
    }

    if (strcmp(event_str, "short") == 0) {
        *out_event = KEY_EVENT_SHORT;
        return true;
    } else if (strcmp(event_str, "long") == 0) {
        *out_event = KEY_EVENT_LONG;
        return true;
    } else if (strcmp(event_str, "repeat") == 0) {
        *out_event = KEY_EVENT_REPEAT;
        return true;
    } else if (strcmp(event_str, "press") == 0) {
        *out_event = KEY_EVENT_PRESS;
        return true;
    } else if (strcmp(event_str, "release") == 0) {
        *out_event = KEY_EVENT_RELEASE;
        return true;
    }

    return false;
}

static bool parse_command_type(const char *cmd_str, AppCommandType *out_type)
{
    if (!cmd_str || !out_type) {
        return false;
    }

    if (strcmp(cmd_str, "clearSafeMode") == 0) {
        *out_type = APP_COMMAND_CLEAR_SAFE_MODE;
        return true;
    } else if (strcmp(cmd_str, "storageManualFlush") == 0) {
        *out_type = APP_COMMAND_STORAGE_MANUAL_FLUSH;
        return true;
    } else if (strcmp(cmd_str, "sensorReinit") == 0) {
        *out_type = APP_COMMAND_SENSOR_REINIT;
        return true;
    } else if (strcmp(cmd_str, "sensorCalibration") == 0) {
        *out_type = APP_COMMAND_SENSOR_CALIBRATION;
        return true;
    } else if (strcmp(cmd_str, "restoreDefaults") == 0) {
        *out_type = APP_COMMAND_RESTORE_DEFAULTS;
        return true;
    }

    return false;
}

static bool read_json_body(AsyncWebServerRequest *request, String &body)
{
    if (request == nullptr) {
        return false;
    }

    if (request->_tempObject != nullptr) {
        body = static_cast<const char *>(request->_tempObject);
        return body.length() > 0;
    }

    body = request->arg("plain");
    if (body.length() > 0) {
        return true;
    }

    if (request->hasParam("body", true)) {
        body = request->getParam("body", true)->value();
        return body.length() > 0;
    }

    return false;
}

static bool read_form_field(AsyncWebServerRequest *request, const char *name, String &value)
{
    if (request == nullptr || name == nullptr) {
        return false;
    }

    if (!request->hasParam(name, true)) {
        if (!request->hasParam(name)) {
            return false;
        }
        value = request->getParam(name)->value();
        return true;
    }

    value = request->getParam(name, true)->value();
    return true;
}

static void capture_request_body(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    if (request == nullptr || data == nullptr || len == 0) {
        return;
    }

    // The API payloads are tiny; keep a modest cap to avoid wasting RAM on bogus posts.
    if (total > 1024U) {
        return;
    }

    if (index == 0 && request->_tempObject == nullptr) {
        request->_tempObject = calloc(total + 1U, sizeof(uint8_t));
        if (request->_tempObject == nullptr) {
            return;
        }
    }

    if (request->_tempObject != nullptr) {
        uint8_t *buffer = static_cast<uint8_t *>(request->_tempObject);
        memcpy(buffer + index, data, len);
    }
}

static void append_hex_byte(String &out, uint8_t value)
{
    out += kHexDigits[(value >> 4) & 0x0F];
    out += kHexDigits[value & 0x0F];
}

void web_register_api_routes(AsyncWebServer &server)
{
    server.on("/api/health", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response = "{";
        response += "\"ok\":true,";
        response += "\"wifiConnected\":";
        response += web_wifi_connected() ? "true" : "false";
        response += ",\"ip\":\"";
        response += web_wifi_ip_string();
        response += "\",\"uptimeMs\":";
        response += (uint64_t)millis();
        response += "}";

        request->send(200, "application/json", response);
    });

    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
        WebStateSnapshot snap;
        if (!web_state_snapshot_collect(&snap)) {
            request->send(500, "application/json", "{\"ok\":false,\"message\":\"snapshot failed\"}");
            return;
        }

        String response = "{";
        response += "\"ok\":true,";

        response += "\"wifi\":{";
        web_json_kv_bool(response, "connected", snap.wifi_connected, false);
        web_json_kv_str(response, "ip", snap.ip, false);
        web_json_kv_i32(response, "rssi", snap.rssi, true);
        response += "},";

        response += "\"system\":{";
        web_json_kv_u32(response, "uptimeMs", snap.uptime_ms, false);
        web_json_kv_str(response, "page", snap.current_page, false);
        web_json_kv_str(response, "timeSource", snap.time_source, false);
        web_json_kv_bool(response, "sleeping", snap.sleeping, false);
        web_json_kv_bool(response, "animating", snap.animating, false);
        web_json_kv_bool(response, "safeMode", snap.safe_mode_active, true);
        response += "},";

        response += "\"activity\":{";
        web_json_kv_u32(response, "steps", snap.steps, false);
        web_json_kv_u32(response, "goal", snap.goal, false);
        web_json_kv_u32(response, "goalPercent", snap.goal_percent, true);
        response += "},";

        response += "\"alarm\":{";
        web_json_kv_u32(response, "nextIndex", snap.next_alarm_index, false);
        web_json_kv_str(response, "nextTime", snap.alarm_time, false);
        web_json_kv_bool(response, "enabled", snap.alarm_enabled, false);
        web_json_kv_bool(response, "ringing", snap.alarm_ringing, false);
        web_json_kv_str(response, "label", snap.alarm_label, true);
        response += "},";

        response += "\"music\":{";
        web_json_kv_bool(response, "available", snap.music_available, false);
        web_json_kv_bool(response, "playing", snap.music_playing, false);
        web_json_kv_str(response, "state", snap.music_state, false);
        web_json_kv_str(response, "song", snap.music_song, false);
        web_json_kv_str(response, "label", snap.music_label, true);
        response += "},";

        response += "\"sensor\":{";
        web_json_kv_bool(response, "online", snap.sensor_online, false);
        web_json_kv_bool(response, "calibrated", snap.sensor_calibrated, false);
        web_json_kv_bool(response, "staticNow", snap.sensor_static_now, false);
        web_json_kv_str(response, "runtimeState", snap.sensor_runtime_state, false);
        web_json_kv_str(response, "calibrationState", snap.sensor_calibration_state, false);
        web_json_kv_u32(response, "quality", snap.sensor_quality, false);
        web_json_kv_u32(response, "errorCode", snap.sensor_error_code, false);
        web_json_kv_u32(response, "faultCount", snap.sensor_fault_count, false);
        web_json_kv_u32(response, "reinitCount", snap.sensor_reinit_count, false);
        web_json_kv_u32(response, "calibrationProgress", snap.sensor_calibration_progress, false);
        web_json_kv_i32(response, "ax", snap.sensor_ax, false);
        web_json_kv_i32(response, "ay", snap.sensor_ay, false);
        web_json_kv_i32(response, "az", snap.sensor_az, false);
        web_json_kv_i32(response, "gx", snap.sensor_gx, false);
        web_json_kv_i32(response, "gy", snap.sensor_gy, false);
        web_json_kv_i32(response, "gz", snap.sensor_gz, false);
        web_json_kv_i32(response, "accelNormMg", snap.sensor_accel_norm_mg, false);
        web_json_kv_i32(response, "baselineMg", snap.sensor_baseline_mg, false);
        web_json_kv_i32(response, "motionScore", snap.sensor_motion_score, false);
        web_json_kv_u32(response, "lastSampleMs", snap.sensor_last_sample_ms, false);
        web_json_kv_u32(response, "stepsTotal", snap.sensor_steps_total, false);
        web_json_kv_f32(response, "pitchDeg", snap.pitch_deg, 2, false);
        web_json_kv_f32(response, "rollDeg", snap.roll_deg, 2, true);
        response += "},";

        response += "\"storage\":{";
        web_json_kv_str(response, "backend", snap.storage_backend, false);
        web_json_kv_str(response, "commitState", snap.storage_commit_state, false);
        web_json_kv_bool(response, "transactionActive", snap.storage_transaction_active, false);
        web_json_kv_bool(response, "sleepFlushPending", snap.storage_sleep_flush_pending, true);
        response += "},";

        response += "\"diag\":{";
        web_json_kv_bool(response, "hasLastFault", snap.has_last_fault, false);
        web_json_kv_str(response, "lastFaultName", snap.last_fault_name, false);
        web_json_kv_str(response, "lastFaultSeverity", snap.last_fault_severity, false);
        web_json_kv_u32(response, "lastFaultCount", snap.last_fault_count, false);
        web_json_kv_bool(response, "hasLastLog", snap.has_last_log, false);
        web_json_kv_str(response, "lastLogName", snap.last_log_name, false);
        web_json_kv_u32(response, "lastLogValue", snap.last_log_value, false);
        web_json_kv_u32(response, "lastLogAux", snap.last_log_aux, true);
        response += "},";

        response += "\"display\":{";
        web_json_kv_u32(response, "presentCount", snap.display_present_count, false);
        web_json_kv_u32(response, "txFailCount", snap.display_tx_fail_count, true);
        response += "},";

        response += "\"weather\":{";
        web_json_kv_bool(response, "valid", snap.weather_valid, false);
        web_json_kv_str(response, "location", snap.weather_location, false);
        web_json_kv_str(response, "text", snap.weather_text, false);
        web_json_kv_f32(response, "temperatureC", (float)snap.weather_temperature_tenths_c / 10.0f, 1, false);
        web_json_kv_u32(response, "updatedAtMs", snap.weather_updated_at_ms, true);
        response += "},";

        response += "\"summary\":{";
        web_json_kv_str(response, "headerTags", snap.header_tags, false);
        web_json_kv_str(response, "networkLine", snap.network_line, false);
        web_json_kv_str(response, "networkSubline", snap.network_subline, false);
        web_json_kv_str(response, "sensorLabel", snap.sensor_label, false);
        web_json_kv_str(response, "storageLabel", snap.storage_label, false);
        web_json_kv_str(response, "diagLabel", snap.diag_label, true);
        response += "},";

        response += "\"terminal\":{";
        web_json_kv_str(response, "systemFace", snap.system_face, false);
        web_json_kv_str(response, "brightnessLabel", snap.brightness_label, false);
        web_json_kv_str(response, "activityLabel", snap.activity_label, false);
        web_json_kv_str(response, "sensorLabel", snap.sensor_label, false);
        web_json_kv_str(response, "networkLabel", snap.network_label, false);
        web_json_kv_str(response, "storageLabel", snap.storage_label, true);
        response += "},";

        response += "\"overlay\":{";
        web_json_kv_bool(response, "active", snap.overlay_active, false);
        web_json_kv_str(response, "text", snap.last_overlay_text, false);
        web_json_kv_u32(response, "expireAtMs", snap.overlay_expire_at_ms, true);
        response += "}";

        response += "}";

        request->send(200, "application/json", response);
    });

    server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    });

    server.on("/api/display/frame", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response;
        response.reserve(96U + (OLED_BUFFER_SIZE * 2U));
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
        response += "\"}";

        request->send(200, "application/json", response);
    });

    server.on("/api/input/key", HTTP_POST, [](AsyncWebServerRequest *request) {
        String key_str;
        String event_str;
        if (read_form_field(request, "key", key_str) && read_form_field(request, "event", event_str)) {
            KeyId key_id = KEY_ID_UP;
            KeyEventType event_type = KEY_EVENT_SHORT;

            if (!parse_key_id(key_str.c_str(), &key_id)) {
                request->send(400, "application/json", "{\"ok\":false,\"message\":\"invalid key\"}");
                return;
            }
            if (!parse_key_event(event_str.c_str(), &event_type)) {
                request->send(400, "application/json", "{\"ok\":false,\"message\":\"invalid event\"}");
                return;
            }

            WebAction action;
            action.type = WEB_ACTION_KEY;
            action.data.key.id = key_id;
            action.data.key.event_type = event_type;

            if (!web_action_enqueue(&action)) {
                request->send(503, "application/json", "{\"ok\":false,\"message\":\"action queue full\"}");
                return;
            }

            String response = "{\"ok\":true,\"message\":\"queued\",\"queueDepth\":";
            response += web_action_queue_depth();
            response += "}";

            request->send(200, "application/json", response);
            return;
        }

        String body;
        if (!read_json_body(request, body)) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"no body\"}");
            return;
        }

        KeyId key_id = KEY_ID_UP;
        KeyEventType event_type = KEY_EVENT_SHORT;
        bool key_valid = false;

        int key_start = body.indexOf("\"key\":");
        if (key_start >= 0) {
            int quote_start = body.indexOf("\"", key_start + 6);
            int quote_end = body.indexOf("\"", quote_start + 1);
            if (quote_start >= 0 && quote_end > quote_start) {
                String key_str = body.substring(quote_start + 1, quote_end);
                key_valid = parse_key_id(key_str.c_str(), &key_id);
            }
        }

        int event_start = body.indexOf("\"event\":");
        if (event_start >= 0) {
            int quote_start = body.indexOf("\"", event_start + 8);
            int quote_end = body.indexOf("\"", quote_start + 1);
            if (quote_start >= 0 && quote_end > quote_start) {
                String event_str = body.substring(quote_start + 1, quote_end);
                parse_key_event(event_str.c_str(), &event_type);
            }
        }

        if (!key_valid) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"invalid key\"}");
            return;
        }

        WebAction action;
        action.type = WEB_ACTION_KEY;
        action.data.key.id = key_id;
        action.data.key.event_type = event_type;

        if (!web_action_enqueue(&action)) {
            request->send(503, "application/json", "{\"ok\":false,\"message\":\"action queue full\"}");
            return;
        }

        String response = "{\"ok\":true,\"message\":\"queued\",\"queueDepth\":";
        response += web_action_queue_depth();
        response += "}";

        request->send(200, "application/json", response);
    }, nullptr, capture_request_body);

    server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest *request) {
        String cmd_str;
        if (read_form_field(request, "type", cmd_str)) {
            AppCommandType cmd_type = (AppCommandType)0;
            if (!parse_command_type(cmd_str.c_str(), &cmd_type) || cmd_type == 0) {
                request->send(400, "application/json", "{\"ok\":false,\"message\":\"invalid command\"}");
                return;
            }

            WebAction action;
            action.type = WEB_ACTION_COMMAND;
            action.data.command.source = APP_COMMAND_SOURCE_UI;
            action.data.command.type = cmd_type;
            action.data.command.data.u32 = 0;

            if (!web_action_enqueue(&action)) {
                request->send(503, "application/json", "{\"ok\":false,\"message\":\"action queue full\"}");
                return;
            }

            String response = "{\"ok\":true,\"message\":\"queued\",\"queueDepth\":";
            response += web_action_queue_depth();
            response += "}";

            request->send(200, "application/json", response);
            return;
        }

        String body;
        if (!read_json_body(request, body)) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"no body\"}");
            return;
        }

        AppCommandType cmd_type = (AppCommandType)0;
        int type_start = body.indexOf("\"type\":");
        if (type_start >= 0) {
            int quote_start = body.indexOf("\"", type_start + 7);
            int quote_end = body.indexOf("\"", quote_start + 1);
            if (quote_start >= 0 && quote_end > quote_start) {
                String cmd_str = body.substring(quote_start + 1, quote_end);
                parse_command_type(cmd_str.c_str(), &cmd_type);
            }
        }

        if (cmd_type == 0) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"invalid command\"}");
            return;
        }

        WebAction action;
        action.type = WEB_ACTION_COMMAND;
        action.data.command.source = APP_COMMAND_SOURCE_UI;
        action.data.command.type = cmd_type;
        action.data.command.data.u32 = 0;

        if (!web_action_enqueue(&action)) {
            request->send(503, "application/json", "{\"ok\":false,\"message\":\"action queue full\"}");
            return;
        }

        String response = "{\"ok\":true,\"message\":\"queued\",\"queueDepth\":";
        response += web_action_queue_depth();
        response += "}";

        request->send(200, "application/json", response);
    }, nullptr, capture_request_body);

    server.on("/api/display/overlay", HTTP_POST, [](AsyncWebServerRequest *request) {
        String overlay_text;
        String duration_str;
        if (read_form_field(request, "text", overlay_text)) {
            if (!read_form_field(request, "durationMs", duration_str)) {
                duration_str = "1500";
            }

            uint32_t duration_ms = (uint32_t)duration_str.toInt();
            if (overlay_text.length() == 0 || overlay_text.length() > 63 || duration_ms == 0) {
                request->send(400, "application/json", "{\"ok\":false,\"message\":\"invalid overlay params\"}");
                return;
            }

            WebAction action;
            action.type = WEB_ACTION_OVERLAY_TEXT;
            strncpy(action.data.overlay.text, overlay_text.c_str(), sizeof(action.data.overlay.text) - 1);
            action.data.overlay.text[sizeof(action.data.overlay.text) - 1] = '\0';
            action.data.overlay.duration_ms = duration_ms;

            if (!web_action_enqueue(&action)) {
                request->send(503, "application/json", "{\"ok\":false,\"message\":\"action queue full\"}");
                return;
            }

            String response = "{\"ok\":true,\"message\":\"queued\"}";
            request->send(200, "application/json", response);
            return;
        }

        String body;
        if (!read_json_body(request, body)) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"no body\"}");
            return;
        }

        overlay_text.clear();
        uint32_t duration_ms = 1500;

        int text_start = body.indexOf("\"text\":");
        if (text_start >= 0) {
            int quote_start = body.indexOf("\"", text_start + 7);
            int quote_end = body.indexOf("\"", quote_start + 1);
            if (quote_start >= 0 && quote_end > quote_start) {
                overlay_text = body.substring(quote_start + 1, quote_end);
            }
        }

        int dur_start = body.indexOf("\"durationMs\":");
        if (dur_start >= 0) {
            int num_start = dur_start + 13;
            int num_end = body.indexOf(",", num_start);
            if (num_end < 0) {
                num_end = body.indexOf("}", num_start);
            }
            if (num_start < num_end) {
                String dur_str = body.substring(num_start, num_end);
                duration_ms = dur_str.toInt();
            }
        }

        if (overlay_text.length() == 0 || overlay_text.length() > 63 || duration_ms == 0) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"invalid overlay params\"}");
            return;
        }

        WebAction action;
        action.type = WEB_ACTION_OVERLAY_TEXT;
        strncpy(action.data.overlay.text, overlay_text.c_str(), sizeof(action.data.overlay.text) - 1);
        action.data.overlay.text[sizeof(action.data.overlay.text) - 1] = '\0';
        action.data.overlay.duration_ms = duration_ms;

        if (!web_action_enqueue(&action)) {
            request->send(503, "application/json", "{\"ok\":false,\"message\":\"action queue full\"}");
            return;
        }

        String response = "{\"ok\":true,\"message\":\"queued\"}";
        request->send(200, "application/json", response);
    }, nullptr, capture_request_body);

    server.on("/api/display/overlay/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        WebAction action;
        action.type = WEB_ACTION_OVERLAY_CLEAR;

        if (!web_action_enqueue(&action)) {
            request->send(503, "application/json", "{\"ok\":false,\"message\":\"action queue full\"}");
            return;
        }

        String response = "{\"ok\":true,\"message\":\"queued\"}";
        request->send(200, "application/json", response);
    });
}
