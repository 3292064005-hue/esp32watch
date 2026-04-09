#include "web/web_server.h"
#include "web/web_wifi.h"
#include "web/web_action_queue.h"
#include "web/web_overlay.h"
#include "web/web_state_bridge.h"
#include "web/web_fs_config.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <string.h>

extern "C" {
#include "key.h"
#include "app_command.h"
}

void web_register_page_routes(AsyncWebServer &server);
void web_register_api_routes(AsyncWebServer &server);

static AsyncWebServer *g_server = nullptr;
static bool g_server_initialized = false;
static bool g_littlefs_ready = false;
static bool g_littlefs_assets_ready = false;
static char g_littlefs_status[24] = "UNINITIALIZED";

static void web_set_filesystem_status(const char *status)
{
    if (status == nullptr) {
        g_littlefs_status[0] = '\0';
        return;
    }
    strncpy(g_littlefs_status, status, sizeof(g_littlefs_status) - 1U);
    g_littlefs_status[sizeof(g_littlefs_status) - 1U] = '\0';
}

static bool web_mount_littlefs(void)
{
    const char *base_path = "/littlefs";
    const char *partition_label = "littlefs";

    g_littlefs_assets_ready = false;
    if (LittleFS.begin(false, base_path, 10, partition_label)) {
        Serial.println("[WEB] LittleFS mounted successfully");
        g_littlefs_assets_ready = LittleFS.exists("/index.html") && LittleFS.exists("/app.js") && LittleFS.exists("/app.css");
        if (!g_littlefs_assets_ready) {
            Serial.println("[WEB] LittleFS mounted but one or more web assets are missing");
            web_set_filesystem_status("ASSET_MISSING");
        } else {
            web_set_filesystem_status("READY");
        }
        return true;
    }

    Serial.println("[WEB] LittleFS mount failed");
#if WEB_LITTLEFS_AUTO_FORMAT_ON_MOUNT_FAILURE
    Serial.println("[WEB] Auto-format is enabled for this build; attempting format + remount...");
    if (!LittleFS.format()) {
        Serial.println("[WEB] LittleFS format failed");
        web_set_filesystem_status("FORMAT_FAIL");
        return false;
    }
    if (!LittleFS.begin(false, base_path, 10, partition_label)) {
        Serial.println("[WEB] LittleFS remount failed after format");
        web_set_filesystem_status("REMOUNT_FAIL");
        return false;
    }
    g_littlefs_assets_ready = LittleFS.exists("/index.html") && LittleFS.exists("/app.js") && LittleFS.exists("/app.css");
    web_set_filesystem_status(g_littlefs_assets_ready ? "RECOVERED" : "ASSET_MISSING");
    return true;
#else
    web_set_filesystem_status("MOUNT_FAILED");
    return false;
#endif
}

bool web_server_init(void)
{
    Serial.println("[WEB] Initializing web server...");

    g_littlefs_ready = web_mount_littlefs();
    if (!g_littlefs_ready) {
        Serial.println("[WEB] LittleFS unavailable; fallback page only");
    }

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
            case WEB_ACTION_KEY:
                key_inject_event(action.data.key.id, action.data.key.event_type);
                Serial.printf("[WEB] Injected key: id=%d, event=%d\n", action.data.key.id, action.data.key.event_type);
                break;

            case WEB_ACTION_COMMAND:
                (void)app_command_execute(&action.data.command, NULL);
                Serial.printf("[WEB] Executed command: type=%d\n", action.data.command.type);
                break;

            case WEB_ACTION_OVERLAY_TEXT:
                (void)web_overlay_request_text(action.data.overlay.text, now_ms, action.data.overlay.duration_ms);
                Serial.printf("[WEB] Overlay text queued: '%s' for %u ms\n", action.data.overlay.text, action.data.overlay.duration_ms);
                break;

            case WEB_ACTION_OVERLAY_CLEAR:
                web_overlay_clear();
                Serial.println("[WEB] Overlay cleared");
                break;

            default:
                break;
        }
    }

    if (!web_overlay_is_active(now_ms)) {
        web_overlay_clear();
    }
}

bool web_server_is_ready(void)
{
    return g_server_initialized && web_wifi_has_network_route();
}

bool web_server_filesystem_ready(void)
{
    return g_littlefs_ready;
}

bool web_server_filesystem_assets_ready(void)
{
    return g_littlefs_assets_ready;
}

const char *web_server_filesystem_status(void)
{
    return g_littlefs_status;
}
