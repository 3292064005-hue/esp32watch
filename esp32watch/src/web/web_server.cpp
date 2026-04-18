#include "web/web_server.h"
#include "web/web_wifi.h"
#include "web/web_action_queue.h"
#include "web/web_overlay.h"
#include "web/web_state_bridge.h"
#include "web/web_fs_config.h"
#include "web/web_contract.h"
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
static bool g_asset_contract_ready = false;
static bool g_asset_contract_hash_verified = false;
static uint32_t g_loaded_asset_contract_version = 0U;
static uint32_t g_loaded_asset_contract_hash = 0U;
static char g_asset_contract_generated_at[32] = "UNKNOWN";
static char g_littlefs_status[24] = "UNINITIALIZED";

typedef struct {
    const char *name;
    char expected_hash[9];
    char actual_hash[9];
} WebAssetHashRecord;

static WebAssetHashRecord g_asset_hashes[] = {
    {"index.html", "UNKNOWN", "UNKNOWN"},
    {"app.js", "UNKNOWN", "UNKNOWN"},
    {"app.css", "UNKNOWN", "UNKNOWN"},
    {"contract-bootstrap.json", "UNKNOWN", "UNKNOWN"},
};

static void web_set_filesystem_status(const char *status)
{
    if (status == nullptr) {
        g_littlefs_status[0] = '\0';
        return;
    }
    strncpy(g_littlefs_status, status, sizeof(g_littlefs_status) - 1U);
    g_littlefs_status[sizeof(g_littlefs_status) - 1U] = '\0';
}

static void web_reset_asset_contract_state(void)
{
    g_asset_contract_ready = false;
    g_asset_contract_hash_verified = false;
    g_loaded_asset_contract_version = 0U;
    g_loaded_asset_contract_hash = 0U;
    strncpy(g_asset_contract_generated_at, "UNKNOWN", sizeof(g_asset_contract_generated_at) - 1U);
    g_asset_contract_generated_at[sizeof(g_asset_contract_generated_at) - 1U] = '\0';
    for (size_t i = 0; i < (sizeof(g_asset_hashes) / sizeof(g_asset_hashes[0])); ++i) {
        strncpy(g_asset_hashes[i].expected_hash, "UNKNOWN", sizeof(g_asset_hashes[i].expected_hash) - 1U);
        g_asset_hashes[i].expected_hash[sizeof(g_asset_hashes[i].expected_hash) - 1U] = '\0';
        strncpy(g_asset_hashes[i].actual_hash, "UNKNOWN", sizeof(g_asset_hashes[i].actual_hash) - 1U);
        g_asset_hashes[i].actual_hash[sizeof(g_asset_hashes[i].actual_hash) - 1U] = '\0';
    }
}

static WebAssetHashRecord *web_find_asset_hash_record(const char *asset_name)
{
    if (asset_name == nullptr || asset_name[0] == '\0') {
        return nullptr;
    }
    for (size_t i = 0; i < (sizeof(g_asset_hashes) / sizeof(g_asset_hashes[0])); ++i) {
        if (strcmp(g_asset_hashes[i].name, asset_name) == 0) {
            return &g_asset_hashes[i];
        }
    }
    return nullptr;
}

static bool web_read_asset_contract(String &out_json)
{
    File contract_file = LittleFS.open("/asset-contract.json", "r");

    out_json.clear();
    if (!contract_file) {
        return false;
    }
    while (contract_file.available()) {
        out_json += (char)contract_file.read();
    }
    contract_file.close();
    return out_json.length() > 0U;
}

static uint32_t web_hash_bytes_fnv1a32(const uint8_t *data, size_t size)
{
    const uint32_t kOffset = 2166136261UL;
    const uint32_t kPrime = 16777619UL;
    uint32_t hash = kOffset;

    if (data == nullptr || size == 0U) {
        return 0U;
    }

    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= kPrime;
    }
    return hash;
}

static uint32_t web_hash_file_fnv1a32(const char *path)
{
    const uint32_t kOffset = 2166136261UL;
    const uint32_t kPrime = 16777619UL;
    File file = LittleFS.open(path, "r");
    uint32_t hash = kOffset;

    if (!file) {
        return 0U;
    }

    while (file.available()) {
        hash ^= (uint8_t)file.read();
        hash *= kPrime;
    }
    file.close();
    return hash;
}

static bool web_contract_find_hash(const String &json, const char *entry_name, char out_hash[9])
{
    String name_needle;
    int entry_index;
    int hash_index;
    int value_index;

    if (entry_name == nullptr || out_hash == nullptr) {
        return false;
    }

    name_needle = "\"";
    name_needle += entry_name;
    name_needle += "\"";
    entry_index = json.indexOf(name_needle.c_str());
    if (entry_index < 0) {
        return false;
    }
    hash_index = json.indexOf("\"fnv1a32\"", entry_index);
    if (hash_index < 0) {
        return false;
    }
    value_index = json.indexOf(':', hash_index);
    if (value_index < 0) {
        return false;
    }
    value_index = json.indexOf('"', value_index);
    if (value_index < 0 || (value_index + 8) >= (int)json.length()) {
        return false;
    }
    ++value_index;
    for (int i = 0; i < 8; ++i) {
        char ch = json[value_index + i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))) {
            return false;
        }
        out_hash[i] = ch;
    }
    out_hash[8] = '\0';
    return true;
}

static bool web_contract_find_string_field(const String &json,
                                           const char *key,
                                           char *out_value,
                                           size_t out_size)
{
    int key_index;
    int value_index;
    int end_index;

    if (key == nullptr || out_value == nullptr || out_size == 0U) {
        return false;
    }

    key_index = json.indexOf(key);
    if (key_index < 0) {
        return false;
    }
    value_index = json.indexOf(':', key_index);
    if (value_index < 0) {
        return false;
    }
    value_index = json.indexOf('"', value_index);
    if (value_index < 0) {
        return false;
    }
    end_index = json.indexOf('"', value_index + 1);
    if (end_index < 0 || end_index <= value_index) {
        return false;
    }
    ++value_index;
    String value = json.substring(value_index, end_index);
    strncpy(out_value, value.c_str(), out_size - 1U);
    out_value[out_size - 1U] = '\0';
    return true;
}

static uint32_t web_parse_asset_contract_version(const String &json)
{
    const char *key = "\"assetContractVersion\"";
    int key_index = json.indexOf(key);
    int value_index;

    if (key_index < 0) {
        return 0U;
    }
    value_index = json.indexOf(':', key_index);
    if (value_index < 0) {
        return 0U;
    }
    while (++value_index < (int)json.length()) {
        char ch = json[value_index];
        if (ch >= '0' && ch <= '9') {
            break;
        }
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            return 0U;
        }
    }
    return (uint32_t)json.substring(value_index, json.length()).toInt();
}

static bool web_validate_asset_contract(void)
{
    String contract_json;
    const char *required_entries[] = {"index.html", "app.js", "app.css", "contract-bootstrap.json"};

    web_reset_asset_contract_state();
    if (!web_read_asset_contract(contract_json)) {
        return false;
    }

    g_loaded_asset_contract_hash = web_hash_bytes_fnv1a32((const uint8_t *)contract_json.c_str(), (size_t)contract_json.length());
    (void)web_contract_find_string_field(contract_json,
                                         "\"generatedAtUtc\"",
                                         g_asset_contract_generated_at,
                                         sizeof(g_asset_contract_generated_at));
    g_loaded_asset_contract_version = web_parse_asset_contract_version(contract_json);
    if (g_loaded_asset_contract_version != WEB_ASSET_CONTRACT_VERSION) {
        Serial.printf("[WEB] Asset contract version mismatch fs=%lu fw=%lu\n",
                      (unsigned long)g_loaded_asset_contract_version,
                      (unsigned long)WEB_ASSET_CONTRACT_VERSION);
        return false;
    }

    for (size_t i = 0; i < (sizeof(required_entries) / sizeof(required_entries[0])); ++i) {
        const char *name = required_entries[i];
        char expected_hash[9];
        char actual_hex[9];
        char path[32];
        uint32_t actual_hash;
        WebAssetHashRecord *record = web_find_asset_hash_record(name);

        if (!web_contract_find_hash(contract_json, name, expected_hash)) {
            Serial.printf("[WEB] Asset contract missing hash for %s\n", name);
            return false;
        }
        snprintf(path, sizeof(path), "/%s", name);
        actual_hash = web_hash_file_fnv1a32(path);
        if (actual_hash == 0U) {
            Serial.printf("[WEB] Asset file missing for %s\n", name);
            return false;
        }
        snprintf(actual_hex, sizeof(actual_hex), "%08lX", (unsigned long)actual_hash);
        if (record != nullptr) {
            strncpy(record->expected_hash, expected_hash, sizeof(record->expected_hash) - 1U);
            record->expected_hash[sizeof(record->expected_hash) - 1U] = '\0';
            strncpy(record->actual_hash, actual_hex, sizeof(record->actual_hash) - 1U);
            record->actual_hash[sizeof(record->actual_hash) - 1U] = '\0';
        }
        if (strcmp(actual_hex, expected_hash) != 0) {
            Serial.printf("[WEB] Asset hash mismatch %s fs=%s expected=%s\n", name, actual_hex, expected_hash);
            return false;
        }
    }

    g_asset_contract_hash_verified = true;
    g_asset_contract_ready = true;
    return true;
}

static bool web_mount_littlefs(void)
{
    const char *base_path = "/littlefs";
    const char *partition_label = "littlefs";

    g_littlefs_assets_ready = false;
    web_reset_asset_contract_state();
    if (LittleFS.begin(false, base_path, 10, partition_label)) {
        Serial.println("[WEB] LittleFS mounted successfully");
        g_littlefs_assets_ready = LittleFS.exists("/index.html") &&
                                  LittleFS.exists("/app.js") &&
                                  LittleFS.exists("/app.css") &&
                                  LittleFS.exists("/contract-bootstrap.json");
        g_asset_contract_ready = web_validate_asset_contract();
        if (!g_littlefs_assets_ready) {
            Serial.println("[WEB] LittleFS mounted but one or more web assets are missing");
            web_set_filesystem_status("ASSET_MISSING");
        } else if (!LittleFS.exists("/asset-contract.json")) {
            Serial.println("[WEB] LittleFS mounted but asset contract file is missing");
            web_set_filesystem_status("ASSET_CONTRACT_MISSING");
        } else if (!g_asset_contract_ready || !g_asset_contract_hash_verified) {
            web_set_filesystem_status("ASSET_CONTRACT_INVALID");
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
    g_littlefs_assets_ready = LittleFS.exists("/index.html") &&
                              LittleFS.exists("/app.js") &&
                              LittleFS.exists("/app.css") &&
                              LittleFS.exists("/contract-bootstrap.json");
    g_asset_contract_ready = web_validate_asset_contract();
    if (!g_littlefs_assets_ready) {
        web_set_filesystem_status("ASSET_MISSING");
    } else if (!LittleFS.exists("/asset-contract.json")) {
        web_set_filesystem_status("ASSET_CONTRACT_MISSING");
    } else if (!g_asset_contract_ready || !g_asset_contract_hash_verified) {
        web_set_filesystem_status("ASSET_CONTRACT_INVALID");
    } else {
        web_set_filesystem_status("RECOVERED");
    }
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

bool web_server_has_pending_actions(void)
{
    return web_action_queue_depth() > 0U;
}

bool web_server_control_plane_ready(void)
{
    return g_server_initialized;
}

bool web_server_console_ready(void)
{
    return g_server_initialized && g_littlefs_ready && g_littlefs_assets_ready && g_asset_contract_ready && g_asset_contract_hash_verified;
}

bool web_server_is_ready(void)
{
    return g_server_initialized;
}

bool web_server_filesystem_ready(void)
{
    return g_littlefs_ready;
}

bool web_server_filesystem_assets_ready(void)
{
    return g_littlefs_assets_ready;
}

bool web_server_asset_contract_ready(void)
{
    return g_asset_contract_ready;
}

uint32_t web_server_asset_contract_version(void)
{
    return g_loaded_asset_contract_version != 0U ? g_loaded_asset_contract_version : WEB_ASSET_CONTRACT_VERSION;
}

uint32_t web_server_asset_contract_hash(void)
{
    return g_loaded_asset_contract_hash;
}

const char *web_server_asset_contract_generated_at(void)
{
    return g_asset_contract_generated_at;
}

const char *web_server_asset_expected_hash(const char *asset_name)
{
    WebAssetHashRecord *record = web_find_asset_hash_record(asset_name);
    return record != nullptr ? record->expected_hash : "UNKNOWN";
}

const char *web_server_asset_actual_hash(const char *asset_name)
{
    WebAssetHashRecord *record = web_find_asset_hash_record(asset_name);
    return record != nullptr ? record->actual_hash : "UNKNOWN";
}

const char *web_server_filesystem_status(void)
{
    return g_littlefs_status;
}

bool web_server_asset_contract_hash_verified(void)
{
    return g_asset_contract_hash_verified;
}
