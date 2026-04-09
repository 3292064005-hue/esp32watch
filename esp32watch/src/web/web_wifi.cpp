#include "web/web_wifi.h"
#include <WiFi.h>
#include <esp_system.h>
#include <string.h>
#include <stdint.h>

extern "C" {
#include "services/device_config.h"
}

namespace {
constexpr uint32_t kWifiConnectTimeoutMs = 15000UL;
constexpr uint32_t kWifiRetryDelayMs = 10000UL;
constexpr uint8_t kProvisioningApMaxConnections = 4U;

enum WebWifiState {
    WEB_WIFI_STATE_IDLE = 0,
    WEB_WIFI_STATE_AP_ONLY,
    WEB_WIFI_STATE_STA_CONNECTING,
    WEB_WIFI_STATE_STA_CONNECTED,
    WEB_WIFI_STATE_STA_RETRY_WAIT,
};

typedef struct {
    WebWifiState state;
    bool initialized;
    bool ap_active;
    bool fallback_ap_enabled;
    uint8_t failed_attempts;
    uint32_t state_enter_at_ms;
    char ip_string[24];
    char status_text[24];
    char ap_ssid[33];
    char ap_password[33];
    int32_t last_rssi;
} WebWifiStateData;

WebWifiStateData g_wifi_state = {
    WEB_WIFI_STATE_IDLE,
    false,
    false,
    false,
    0U,
    0U,
    "",
    "IDLE",
    "",
    "",
    -100,
};

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst == nullptr || dst_size == 0U) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static void set_status(const char *status)
{
    copy_string(g_wifi_state.status_text, sizeof(g_wifi_state.status_text), status);
}

static void refresh_ip_string(void)
{
    IPAddress ip = g_wifi_state.ap_active && !web_wifi_connected() ? WiFi.softAPIP() : WiFi.localIP();
    snprintf(g_wifi_state.ip_string,
             sizeof(g_wifi_state.ip_string),
             "%u.%u.%u.%u",
             ip[0], ip[1], ip[2], ip[3]);
}

static void stop_provisioning_ap(void)
{
    if (!g_wifi_state.ap_active) {
        return;
    }
    WiFi.softAPdisconnect(true);
    g_wifi_state.ap_active = false;
    g_wifi_state.fallback_ap_enabled = false;
}

static bool start_provisioning_ap(void)
{
    uint64_t chip_id = ESP.getEfuseMac();

    if (g_wifi_state.ap_active) {
        return true;
    }

    snprintf(g_wifi_state.ap_ssid,
             sizeof(g_wifi_state.ap_ssid),
             "ESP32Watch-%04X",
             (unsigned)(chip_id & 0xFFFFU));
    uint32_t secret = (uint32_t)(((chip_id >> 24) ^ (chip_id >> 8) ^ chip_id ^ 0x5A17A5U) & 0xFFFFFFU);
    snprintf(g_wifi_state.ap_password,
             sizeof(g_wifi_state.ap_password),
             "watch-%06X",
             (unsigned)secret);

    if (!WiFi.softAP(g_wifi_state.ap_ssid, g_wifi_state.ap_password, 1, false, kProvisioningApMaxConnections)) {
        set_status("AP_FAIL");
        return false;
    }

    g_wifi_state.ap_active = true;
    g_wifi_state.fallback_ap_enabled = true;
    refresh_ip_string();
    set_status("AP_READY");
    Serial.printf("[WIFI] Provisioning AP active: ssid=%s password=%s ip=%s\n",
                  g_wifi_state.ap_ssid,
                  g_wifi_state.ap_password,
                  g_wifi_state.ip_string);
    return true;
}

static bool wifi_has_credentials(DeviceConfigSnapshot *cfg)
{
    return cfg != nullptr && cfg->wifi_configured && cfg->wifi_ssid[0] != '\0';
}

static bool begin_station_connect(const DeviceConfigSnapshot &cfg, uint32_t now_ms)
{
    char password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};

    if (!wifi_has_credentials(const_cast<DeviceConfigSnapshot *>(&cfg))) {
        return false;
    }

    (void)device_config_get_wifi_password(password, sizeof(password));
    WiFi.mode(g_wifi_state.ap_active ? WIFI_AP_STA : WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.persistent(false);
    WiFi.disconnect(true, false);
    delay(20);
    WiFi.begin(cfg.wifi_ssid, password);

    g_wifi_state.state = WEB_WIFI_STATE_STA_CONNECTING;
    g_wifi_state.state_enter_at_ms = now_ms;
    set_status("CONNECTING");
    Serial.printf("[WIFI] Connecting to SSID '%s'\n", cfg.wifi_ssid);
    return true;
}

static void enter_ap_only(uint32_t now_ms)
{
    WiFi.mode(WIFI_AP);
    start_provisioning_ap();
    g_wifi_state.state = WEB_WIFI_STATE_AP_ONLY;
    g_wifi_state.state_enter_at_ms = now_ms;
    g_wifi_state.last_rssi = -100;
    refresh_ip_string();
}

static void restart_from_config(uint32_t now_ms)
{
    DeviceConfigSnapshot cfg;

    device_config_init();
    (void)device_config_get(&cfg);
    WiFi.disconnect(true, true);
    stop_provisioning_ap();
    memset(g_wifi_state.ip_string, 0, sizeof(g_wifi_state.ip_string));
    g_wifi_state.failed_attempts = 0U;
    g_wifi_state.last_rssi = -100;

    if (!wifi_has_credentials(&cfg)) {
        enter_ap_only(now_ms);
        return;
    }

    g_wifi_state.state = WEB_WIFI_STATE_IDLE;
    g_wifi_state.state_enter_at_ms = now_ms;
    if (!begin_station_connect(cfg, now_ms)) {
        enter_ap_only(now_ms);
    }
}
} // namespace

extern "C" bool web_wifi_init(void)
{
    if (g_wifi_state.initialized) {
        return true;
    }

    memset(&g_wifi_state, 0, sizeof(g_wifi_state));
    g_wifi_state.state = WEB_WIFI_STATE_IDLE;
    g_wifi_state.last_rssi = -100;
    set_status("INIT");
    device_config_init();
    restart_from_config(millis());
    g_wifi_state.initialized = true;
    return true;
}

extern "C" void web_wifi_tick(uint32_t now_ms)
{
    DeviceConfigSnapshot cfg;
    wl_status_t wifi_status;

    if (!g_wifi_state.initialized) {
        return;
    }

    (void)device_config_get(&cfg);
    wifi_status = WiFi.status();

    switch (g_wifi_state.state) {
        case WEB_WIFI_STATE_AP_ONLY:
            refresh_ip_string();
            break;
        case WEB_WIFI_STATE_STA_CONNECTING:
            if (wifi_status == WL_CONNECTED) {
                g_wifi_state.state = WEB_WIFI_STATE_STA_CONNECTED;
                g_wifi_state.state_enter_at_ms = now_ms;
                g_wifi_state.failed_attempts = 0U;
                g_wifi_state.last_rssi = WiFi.RSSI();
                refresh_ip_string();
                stop_provisioning_ap();
                set_status("CONNECTED");
                Serial.printf("[WIFI] Connected, ip=%s rssi=%d\n",
                              g_wifi_state.ip_string,
                              g_wifi_state.last_rssi);
                break;
            }
            if ((now_ms - g_wifi_state.state_enter_at_ms) >= kWifiConnectTimeoutMs) {
                g_wifi_state.failed_attempts++;
                if (g_wifi_state.failed_attempts >= 2U) {
                    WiFi.mode(WIFI_AP_STA);
                    (void)start_provisioning_ap();
                }
                g_wifi_state.state = WEB_WIFI_STATE_STA_RETRY_WAIT;
                g_wifi_state.state_enter_at_ms = now_ms;
                set_status(g_wifi_state.ap_active ? "RETRY_AP" : "RETRY");
                Serial.printf("[WIFI] Connect timeout status=%d failed_attempts=%u\n",
                              (int)wifi_status,
                              g_wifi_state.failed_attempts);
            }
            break;
        case WEB_WIFI_STATE_STA_CONNECTED:
            if (wifi_status != WL_CONNECTED) {
                g_wifi_state.state = WEB_WIFI_STATE_STA_CONNECTING;
                g_wifi_state.state_enter_at_ms = now_ms;
                set_status("RECONNECT");
                begin_station_connect(cfg, now_ms);
            } else {
                g_wifi_state.last_rssi = WiFi.RSSI();
                refresh_ip_string();
            }
            break;
        case WEB_WIFI_STATE_STA_RETRY_WAIT:
            if ((now_ms - g_wifi_state.state_enter_at_ms) >= kWifiRetryDelayMs) {
                if (!begin_station_connect(cfg, now_ms)) {
                    enter_ap_only(now_ms);
                }
            }
            break;
        case WEB_WIFI_STATE_IDLE:
        default:
            restart_from_config(now_ms);
            break;
    }
}

extern "C" bool web_wifi_reconfigure(void)
{
    if (!g_wifi_state.initialized) {
        return false;
    }
    restart_from_config(millis());
    return true;
}

extern "C" bool web_wifi_connected(void)
{
    return g_wifi_state.state == WEB_WIFI_STATE_STA_CONNECTED && WiFi.status() == WL_CONNECTED;
}

extern "C" const char *web_wifi_ip_string(void)
{
    return g_wifi_state.ip_string;
}

extern "C" int32_t web_wifi_rssi(void)
{
    return g_wifi_state.last_rssi;
}

extern "C" bool web_wifi_access_point_active(void)
{
    return g_wifi_state.ap_active;
}

extern "C" const char *web_wifi_access_point_ssid(void)
{
    return g_wifi_state.ap_ssid;
}

extern "C" const char *web_wifi_access_point_password(void)
{
    return g_wifi_state.ap_password;
}

extern "C" const char *web_wifi_mode_name(void)
{
    switch (g_wifi_state.state) {
        case WEB_WIFI_STATE_AP_ONLY: return "AP";
        case WEB_WIFI_STATE_STA_CONNECTING: return g_wifi_state.ap_active ? "AP+STA" : "STA";
        case WEB_WIFI_STATE_STA_CONNECTED: return "STA";
        case WEB_WIFI_STATE_STA_RETRY_WAIT: return g_wifi_state.ap_active ? "AP+STA" : "STA";
        default: return "IDLE";
    }
}

extern "C" const char *web_wifi_status_name(void)
{
    return g_wifi_state.status_text;
}

extern "C" bool web_wifi_has_network_route(void)
{
    return web_wifi_connected() || web_wifi_access_point_active();
}
