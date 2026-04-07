#include "web/web_wifi.h"
#include <WiFi.h>
#include <string.h>

#define WIFI_SSID "REDMI K80"
#define WIFI_PASSWORD "12345678"
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_DELAY_MS 10000

typedef enum {
    WEB_WIFI_IDLE = 0,
    WEB_WIFI_CONNECTING,
    WEB_WIFI_CONNECTED,
    WEB_WIFI_RETRY_WAIT
} WebWifiState;

typedef struct {
    WebWifiState state;
    uint32_t state_enter_at_ms;
    char ip_string[24];
    int32_t last_rssi;
} WebWifiStateData;

static WebWifiStateData g_wifi_state = {
    WEB_WIFI_IDLE,
    0,
    "",
    -100
};

bool web_wifi_init(void)
{
    Serial.println("[WIFI] Init starting...");
    Serial.printf("[WIFI] SSID: %s\n", WIFI_SSID);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);

    g_wifi_state.state = WEB_WIFI_CONNECTING;
    g_wifi_state.state_enter_at_ms = millis();

    Serial.println("[WIFI] Calling WiFi.begin()");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.println("[WIFI] Init completed, waiting for connection");
    return true;
}

void web_wifi_tick(uint32_t now_ms)
{
    switch (g_wifi_state.state) {
        case WEB_WIFI_IDLE:
            break;

        case WEB_WIFI_CONNECTING: {
            wl_status_t status = WiFi.status();
            if (status == WL_CONNECTED) {
                g_wifi_state.state = WEB_WIFI_CONNECTED;
                g_wifi_state.state_enter_at_ms = now_ms;

                IPAddress ip = WiFi.localIP();
                snprintf(g_wifi_state.ip_string, sizeof(g_wifi_state.ip_string),
                         "%d.%d.%d.%d",
                         ip[0], ip[1], ip[2], ip[3]);

                Serial.printf("[WIFI] ✅ Connected to '%s', IP: %s, RSSI: %d dBm\n", 
                              WIFI_SSID, g_wifi_state.ip_string, WiFi.RSSI());
            } else if (now_ms - g_wifi_state.state_enter_at_ms > WIFI_CONNECT_TIMEOUT_MS) {
                g_wifi_state.state = WEB_WIFI_RETRY_WAIT;
                g_wifi_state.state_enter_at_ms = now_ms;
                Serial.printf("[WIFI] ⏱️  Timeout after %u ms (status=%d: %s). Retrying in %u ms\n",
                              WIFI_CONNECT_TIMEOUT_MS, status,
                              status == WL_DISCONNECTED ? "DISCONNECTED" :
                              status == WL_NO_SSID_AVAIL ? "SSID_NOT_FOUND" :
                              status == WL_CONNECT_FAILED ? "CONNECT_FAILED" :
                              status == WL_IDLE_STATUS ? "IDLE" : "UNKNOWN",
                              WIFI_RETRY_DELAY_MS);
            } else if ((now_ms - g_wifi_state.state_enter_at_ms) % 3000 == 0) {
                Serial.printf("[WIFI] Connecting... %u ms (status=%d)\n",
                              now_ms - g_wifi_state.state_enter_at_ms, status);
            }
            break;
        }

        case WEB_WIFI_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                g_wifi_state.state = WEB_WIFI_CONNECTING;
                g_wifi_state.state_enter_at_ms = now_ms;
                Serial.println("[WIFI] ⚠️  Connection lost, reconnecting...");
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            } else {
                g_wifi_state.last_rssi = WiFi.RSSI();
            }
            break;

        case WEB_WIFI_RETRY_WAIT:
            if (now_ms - g_wifi_state.state_enter_at_ms > WIFI_RETRY_DELAY_MS) {
                g_wifi_state.state = WEB_WIFI_CONNECTING;
                g_wifi_state.state_enter_at_ms = now_ms;
                Serial.println("[WIFI] 🔄 Retrying connection...");
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            }
            break;

        default:
            break;
    }
}

bool web_wifi_connected(void)
{
    return g_wifi_state.state == WEB_WIFI_CONNECTED && WiFi.status() == WL_CONNECTED;
}

const char *web_wifi_ip_string(void)
{
    return g_wifi_state.ip_string;
}

int32_t web_wifi_rssi(void)
{
    return g_wifi_state.last_rssi;
}
