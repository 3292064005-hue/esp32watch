#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <math.h>
#include <string.h>

extern "C" {
#include "services/network_sync_service.h"
#include "services/time_service.h"
#include "web/web_wifi.h"
}

static const char *kTzBeijing = "CST-8";
static const char *kNtpServer1 = "ntp.aliyun.com";
static const char *kNtpServer2 = "ntp.ntsc.ac.cn";
static const char *kNtpServer3 = "pool.ntp.org";
static const char *kWeatherUrl =
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=43.8573"
    "&longitude=125.3366"
    "&current=temperature_2m,weather_code"
    "&timezone=Asia%2FShanghai"
    "&forecast_days=1";

static const uint32_t kTimeSyncRetryMs = 5000UL;
static const uint32_t kTimeSyncPeriodMs = 6UL * 60UL * 60UL * 1000UL;
static const uint32_t kWeatherRetryMs = 60000UL;
static const uint32_t kWeatherPeriodMs = 30UL * 60UL * 1000UL;

typedef struct {
    NetworkSyncSnapshot snapshot;
    bool ntp_started;
    bool force_refresh;
    uint32_t last_time_attempt_ms;
    uint32_t last_weather_attempt_ms;
} NetworkSyncState;

static NetworkSyncState g_network_sync;

static bool json_extract_number(const String &json, const char *key, float *out_value)
{
    int start;
    int end;

    if (key == nullptr || out_value == nullptr) {
        return false;
    }

    start = json.indexOf(key);
    if (start < 0) {
        return false;
    }
    start += (int)strlen(key);

    while (start < (int)json.length() && (json[start] == ' ' || json[start] == '\t')) {
        start++;
    }

    end = start;
    while (end < (int)json.length()) {
        char ch = json[end];
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.') {
            end++;
        } else {
            break;
        }
    }

    if (end <= start) {
        return false;
    }

    *out_value = json.substring(start, end).toFloat();
    return true;
}

static bool network_sync_try_time_sync(uint32_t now_ms)
{
    struct tm local_tm;
    DateTime dt;

    if (!g_network_sync.ntp_started) {
        configTzTime(kTzBeijing, kNtpServer1, kNtpServer2, kNtpServer3);
        g_network_sync.ntp_started = true;
    }

    if (!getLocalTime(&local_tm, 50)) {
        return false;
    }

    dt.year = (uint16_t)(local_tm.tm_year + 1900);
    dt.month = (uint8_t)(local_tm.tm_mon + 1);
    dt.day = (uint8_t)local_tm.tm_mday;
    dt.weekday = (uint8_t)local_tm.tm_wday;
    dt.hour = (uint8_t)local_tm.tm_hour;
    dt.minute = (uint8_t)local_tm.tm_min;
    dt.second = (uint8_t)local_tm.tm_sec;

    if (!time_service_try_set_datetime(&dt, TIME_SOURCE_NETWORK_SYNC)) {
        return false;
    }

    g_network_sync.snapshot.time_synced = true;
    g_network_sync.snapshot.last_time_sync_ms = now_ms;
    Serial.printf("[NET] Time synced: %04u-%02u-%02u %02u:%02u:%02u CST\n",
                  (unsigned)dt.year,
                  (unsigned)dt.month,
                  (unsigned)dt.day,
                  (unsigned)dt.hour,
                  (unsigned)dt.minute,
                  (unsigned)dt.second);
    return true;
}

static bool network_sync_fetch_weather(uint32_t now_ms)
{
    WiFiClientSecure client;
    HTTPClient http;
    String payload;
    String current_json;
    float temp_c = 0.0f;
    float weather_code_f = 0.0f;
    int current_pos;
    int status_code;

    client.setInsecure();
    http.setConnectTimeout(4000);
    http.setTimeout(4000);

    if (!http.begin(client, kWeatherUrl)) {
        return false;
    }

    status_code = http.GET();
    if (status_code != HTTP_CODE_OK) {
        Serial.printf("[NET] Weather HTTP error: %d\n", status_code);
        http.end();
        return false;
    }

    payload = http.getString();
    http.end();

    current_pos = payload.indexOf("\"current\":");
    if (current_pos < 0) {
        Serial.println("[NET] Weather payload missing current object");
        return false;
    }
    current_json = payload.substring(current_pos);

    if (!json_extract_number(current_json, "\"temperature_2m\":", &temp_c) ||
        !json_extract_number(current_json, "\"weather_code\":", &weather_code_f)) {
        Serial.println("[NET] Weather payload parse failed");
        return false;
    }

    g_network_sync.snapshot.weather_valid = true;
    g_network_sync.snapshot.temperature_tenths_c = (int16_t)lroundf(temp_c * 10.0f);
    g_network_sync.snapshot.weather_code = (uint8_t)weather_code_f;
    strncpy(g_network_sync.snapshot.weather_text,
            network_sync_service_weather_text(g_network_sync.snapshot.weather_code),
            sizeof(g_network_sync.snapshot.weather_text) - 1);
    g_network_sync.snapshot.weather_text[sizeof(g_network_sync.snapshot.weather_text) - 1] = '\0';
    g_network_sync.snapshot.last_weather_sync_ms = now_ms;

    Serial.printf("[NET] Weather synced: %s %d.%dC code=%u\n",
                  g_network_sync.snapshot.weather_text,
                  (int)(g_network_sync.snapshot.temperature_tenths_c / 10),
                  abs((int)(g_network_sync.snapshot.temperature_tenths_c % 10)),
                  (unsigned)g_network_sync.snapshot.weather_code);
    return true;
}

extern "C" void network_sync_service_init(void)
{
    memset(&g_network_sync, 0, sizeof(g_network_sync));
    strncpy(g_network_sync.snapshot.location_name, "NANGUAN", sizeof(g_network_sync.snapshot.location_name) - 1);
}

extern "C" void network_sync_service_tick(uint32_t now_ms)
{
    bool force_refresh;
    bool want_time_sync;
    bool want_weather_sync;

    g_network_sync.snapshot.wifi_connected = web_wifi_connected();
    if (!g_network_sync.snapshot.wifi_connected) {
        return;
    }

    force_refresh = g_network_sync.force_refresh;
    want_time_sync = force_refresh ||
                     !g_network_sync.snapshot.time_synced ||
                     (now_ms - g_network_sync.snapshot.last_time_sync_ms) >= kTimeSyncPeriodMs;
    if (want_time_sync && (force_refresh || (now_ms - g_network_sync.last_time_attempt_ms) >= kTimeSyncRetryMs)) {
        g_network_sync.last_time_attempt_ms = now_ms;
        (void)network_sync_try_time_sync(now_ms);
    }

    want_weather_sync = force_refresh ||
                        !g_network_sync.snapshot.weather_valid ||
                        (now_ms - g_network_sync.snapshot.last_weather_sync_ms) >= kWeatherPeriodMs;
    if (want_weather_sync && (force_refresh || (now_ms - g_network_sync.last_weather_attempt_ms) >= kWeatherRetryMs)) {
        g_network_sync.last_weather_attempt_ms = now_ms;
        (void)network_sync_fetch_weather(now_ms);
    }

    g_network_sync.force_refresh = false;
}

extern "C" void network_sync_service_request_refresh(void)
{
    g_network_sync.force_refresh = true;
}

extern "C" bool network_sync_service_get_snapshot(NetworkSyncSnapshot *out)
{
    if (out == nullptr) {
        return false;
    }
    *out = g_network_sync.snapshot;
    return true;
}

extern "C" const char *network_sync_service_weather_text(uint8_t weather_code)
{
    switch (weather_code) {
        case 0: return "CLEAR";
        case 1: return "MAINLY CLR";
        case 2: return "PART CLOUD";
        case 3: return "OVERCAST";
        case 45:
        case 48: return "FOG";
        case 51:
        case 53:
        case 55: return "DRIZZLE";
        case 56:
        case 57: return "FRZ DRIZ";
        case 61:
        case 63:
        case 65: return "RAIN";
        case 66:
        case 67: return "FRZ RAIN";
        case 71:
        case 73:
        case 75:
        case 77: return "SNOW";
        case 80:
        case 81:
        case 82: return "SHOWERS";
        case 85:
        case 86: return "SNOW SHWR";
        case 95: return "TSTORM";
        case 96:
        case 99: return "TSTRM HAIL";
        default: return "UNKNOWN";
    }
}
