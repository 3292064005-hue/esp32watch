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
        web_action_mark_running(action.id, now_ms);

        switch (action.type) {
            case WEB_ACTION_KEY:
                key_inject_event(action.data.key.id, action.data.key.event_type);
                web_action_mark_completed(action.id,
                                          WEB_ACTION_STATUS_APPLIED,
                                          true,
                                          APP_COMMAND_RESULT_OK,
                                          now_ms,
                                          "key injected");
                Serial.printf("[WEB] key action=%lu id=%d event=%d\n",
                              (unsigned long)action.id,
                              action.data.key.id,
                              action.data.key.event_type);
                break;

            case WEB_ACTION_COMMAND: {
                AppCommandExecutionResult result = {0};
                bool ok = app_command_execute_detailed(&action.data.command, NULL, &result);
                WebActionStatus final_status = ok ? WEB_ACTION_STATUS_APPLIED :
                    ((result.code == APP_COMMAND_RESULT_BLOCKED ||
                      result.code == APP_COMMAND_RESULT_OUT_OF_RANGE ||
                      result.code == APP_COMMAND_RESULT_INVALID_DESCRIPTOR ||
                      result.code == APP_COMMAND_RESULT_INVALID_SOURCE ||
                      result.code == APP_COMMAND_RESULT_UNSUPPORTED)
                         ? WEB_ACTION_STATUS_REJECTED
                         : WEB_ACTION_STATUS_FAILED);
                web_action_mark_completed(action.id,
                                          final_status,
                                          result.ok,
                                          result.code,
                                          now_ms,
                                          ok ? "command applied" : app_command_result_code_name(result.code));
                Serial.printf("[WEB] command action=%lu type=%s result=%s\n",
                              (unsigned long)action.id,
                              app_command_describe(action.data.command.type) != NULL ? app_command_describe(action.data.command.type)->wire_name : "unknown",
                              app_command_result_code_name(result.code));
                break;
            }

            case WEB_ACTION_OVERLAY_TEXT: {
                bool ok = web_overlay_request_text(action.data.overlay.text, now_ms, action.data.overlay.duration_ms);
                web_action_mark_completed(action.id,
                                          ok ? WEB_ACTION_STATUS_APPLIED : WEB_ACTION_STATUS_FAILED,
                                          ok,
                                          ok ? APP_COMMAND_RESULT_OK : APP_COMMAND_RESULT_BACKEND_FAILURE,
                                          now_ms,
                                          ok ? "overlay shown" : "overlay rejected");
                Serial.printf("[WEB] overlay action=%lu duration=%u text='%s'\n",
                              (unsigned long)action.id,
                              action.data.overlay.duration_ms,
                              action.data.overlay.text);
                break;
            }

            case WEB_ACTION_OVERLAY_CLEAR:
                web_overlay_clear();
                web_action_mark_completed(action.id,
                                          WEB_ACTION_STATUS_APPLIED,
                                          true,
                                          APP_COMMAND_RESULT_OK,
                                          now_ms,
                                          "overlay cleared");
                Serial.printf("[WEB] overlay clear action=%lu\n", (unsigned long)action.id);
                break;

            default:
                web_action_mark_completed(action.id,
                                          WEB_ACTION_STATUS_FAILED,
                                          false,
                                          APP_COMMAND_RESULT_UNSUPPORTED,
                                          now_ms,
                                          "unknown action");
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
