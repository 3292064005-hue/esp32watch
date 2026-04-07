#include "web/web_server.h"
#include "web/web_wifi.h"
#include "web/web_action_queue.h"
#include "web/web_overlay.h"
#include "web/web_state_bridge.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

extern "C" {
#include "key.h"
#include "app_command.h"
}

void web_register_page_routes(AsyncWebServer &server);
void web_register_api_routes(AsyncWebServer &server);

static AsyncWebServer *g_server = nullptr;
static bool g_server_initialized = false;

extern void web_state_bridge_mark_startup(uint32_t mark_ms);

static bool web_mount_littlefs(void)
{
    const char *base_path = "/littlefs";
    const char *partition_label = "littlefs";

    if (LittleFS.begin(false, base_path, 10, partition_label)) {
        Serial.println("[WEB] LittleFS mounted successfully");
        if (!LittleFS.exists("/index.html")) {
            Serial.println("[WEB] Warning: /index.html missing in LittleFS");
        }
        return true;
    }

    Serial.println("[WEB] LittleFS mount failed, attempting format + remount...");
    if (!LittleFS.format()) {
        Serial.println("[WEB] LittleFS format failed");
        return false;
    }

    if (!LittleFS.begin(false, base_path, 10, partition_label)) {
        Serial.println("[WEB] LittleFS remount failed after format");
        return false;
    }

    Serial.println("[WEB] LittleFS recovered by format + remount");
    return true;
}

bool web_server_init(void)
{
    Serial.println("[WEB] Initializing web server...");

    (void)web_mount_littlefs();

    if (!web_wifi_init()) {
        Serial.println("[WEB] WiFi init failed!");
        return false;
    }

    Serial.println("[WEB] WiFi init completed");

    if (g_server == nullptr) {
        g_server = new AsyncWebServer(80);
        if (g_server == nullptr) {
            Serial.println("[WEB] AsyncWebServer allocation failed!");
            return false;
        }
    }

    // Register routes even if LittleFS is unavailable.
    web_register_page_routes(*g_server);
    web_register_api_routes(*g_server);

    g_server->begin();
    Serial.println("[WEB] HTTP server started on port 80");

    g_server_initialized = true;

    web_state_bridge_mark_startup(millis());

    return true;
}

void web_server_poll(uint32_t now_ms)
{
    if (!g_server_initialized) {
        return;
    }

    web_wifi_tick(now_ms);

    WebAction action;
    while (web_action_dequeue(&action)) {
        switch (action.type) {
            case WEB_ACTION_KEY: {
                key_inject_event(action.data.key.id, action.data.key.event_type);
                Serial.printf("[WEB] Injected key: id=%d, event=%d\n",
                              action.data.key.id, action.data.key.event_type);
                break;
            }

            case WEB_ACTION_COMMAND: {
                (void)app_command_execute(&action.data.command, NULL);
                Serial.printf("[WEB] Executed command: type=%d\n", action.data.command.type);
                break;
            }

            case WEB_ACTION_OVERLAY_TEXT: {
                (void)web_overlay_request_text(
                    action.data.overlay.text,
                    now_ms,
                    action.data.overlay.duration_ms
                );
                Serial.printf("[WEB] Overlay text queued: '%s' for %u ms\n",
                              action.data.overlay.text, action.data.overlay.duration_ms);
                break;
            }

            case WEB_ACTION_OVERLAY_CLEAR: {
                web_overlay_clear();
                Serial.println("[WEB] Overlay cleared");
                break;
            }

            default:
                break;
        }
    }

    if (web_overlay_is_active(now_ms) == false) {
        web_overlay_clear();
    }
}

bool web_server_is_ready(void)
{
    return g_server_initialized && web_wifi_connected();
}
