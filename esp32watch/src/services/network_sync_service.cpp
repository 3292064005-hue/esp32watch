#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {
#include "services/device_config.h"
#include "services/network_sync_service.h"
#include "services/network_sync_build_config.h"
#include "services/time_service.h"
#include "web/web_wifi.h"
}

namespace {
constexpr const char *kNtpServer1 = "ntp.aliyun.com";
constexpr const char *kNtpServer2 = "ntp.ntsc.ac.cn";
constexpr const char *kNtpServer3 = "pool.ntp.org";
constexpr const char *kWeatherBaseUrl = "https://api.open-meteo.com/v1/forecast";

extern const char kWeatherCaBundleStart[] asm("_binary_data_certs_weather_ca_bundle_pem_start");
extern const char kWeatherCaBundleEnd[] asm("_binary_data_certs_weather_ca_bundle_pem_end");

struct NetworkSyncState {
    NetworkSyncSnapshot snapshot;
    bool ntp_started;
    bool force_refresh;
    uint32_t last_time_attempt_ms;
    uint32_t last_weather_attempt_ms;
    char active_tz_posix[DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN + 1U];
    char weather_tz_id[DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN + 1U];
    char weather_location_name[DEVICE_CONFIG_LOCATION_NAME_MAX_LEN + 1U];
    float weather_latitude;
    float weather_longitude;
};

NetworkSyncState g_network_sync = {};

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

static void set_weather_status(const char *status)
{
    copy_string(g_network_sync.snapshot.weather_status,
                sizeof(g_network_sync.snapshot.weather_status),
                status);
}

static void update_tls_snapshot(bool strict_mode, bool ca_loaded)
{
    g_network_sync.snapshot.weather_ca_loaded = ca_loaded;
    g_network_sync.snapshot.weather_tls_verified = strict_mode && ca_loaded;
    copy_string(g_network_sync.snapshot.weather_tls_mode,
                sizeof(g_network_sync.snapshot.weather_tls_mode),
                strict_mode ? "STRICT" : "INSECURE");
}

static const char *weather_ca_bundle_ptr(size_t *out_len)
{
    size_t len = 0U;
    if (kWeatherCaBundleEnd > kWeatherCaBundleStart) {
        len = (size_t)(kWeatherCaBundleEnd - kWeatherCaBundleStart);
    }
    if (out_len != nullptr) {
        *out_len = len;
    }
    return len > 1U ? kWeatherCaBundleStart : nullptr;
}

static bool parse_weather_payload(const String &payload, float *out_temp_c, uint8_t *out_weather_code)
{
    StaticJsonDocument<1024> doc;
    JsonVariant current;
    float temp_c;
    int weather_code;

    if (out_temp_c == nullptr || out_weather_code == nullptr) {
        return false;
    }

    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[NET] Weather JSON parse error: %s\n", err.c_str());
        return false;
    }

    current = doc["current"];
    if (current.isNull() || !current.is<JsonObject>()) {
        Serial.println("[NET] Weather payload missing current object");
        return false;
    }

    if (!current["temperature_2m"].is<float>() && !current["temperature_2m"].is<double>() && !current["temperature_2m"].is<int>()) {
        Serial.println("[NET] Weather payload missing temperature_2m");
        return false;
    }
    if (!current["weather_code"].is<int>()) {
        Serial.println("[NET] Weather payload missing weather_code");
        return false;
    }

    temp_c = current["temperature_2m"].as<float>();
    weather_code = current["weather_code"].as<int>();
    if (!isfinite(temp_c) || weather_code < 0 || weather_code > 255) {
        Serial.println("[NET] Weather payload contained invalid values");
        return false;
    }

    *out_temp_c = temp_c;
    *out_weather_code = (uint8_t)weather_code;
    return true;
}

static void append_url_encoded(String &dst, const char *src)
{
    static const char *kHex = "0123456789ABCDEF";

    if (src == nullptr) {
        return;
    }
    while (*src != '\0') {
        uint8_t ch = (uint8_t)*src++;
        bool safe = (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '-' || ch == '_' || ch == '.' || ch == '~';
        if (safe) {
            dst += (char)ch;
            continue;
        }
        dst += '%';
        dst += kHex[(ch >> 4) & 0x0F];
        dst += kHex[ch & 0x0F];
    }
}

static void clear_weather_snapshot(const DeviceConfigSnapshot &cfg)
{
    g_network_sync.snapshot.weather_valid = false;
    g_network_sync.snapshot.temperature_tenths_c = 0;
    g_network_sync.snapshot.weather_code = 0U;
    g_network_sync.snapshot.last_weather_sync_ms = 0U;
    g_network_sync.snapshot.weather_last_attempt_ok = false;
    g_network_sync.snapshot.last_weather_http_status = 0;
    copy_string(g_network_sync.snapshot.weather_text,
                sizeof(g_network_sync.snapshot.weather_text),
                cfg.weather_configured ? "SYNC NEEDED" : "UNSET");
    copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                cfg.location_name);
    set_weather_status(cfg.weather_configured ? "PENDING" : "UNSET");
}

static bool weather_profile_changed(const DeviceConfigSnapshot &cfg)
{
    return strcmp(g_network_sync.weather_tz_id, cfg.timezone_id) != 0 ||
           strcmp(g_network_sync.weather_location_name, cfg.location_name) != 0 ||
           fabsf(g_network_sync.weather_latitude - cfg.latitude) > 0.000001f ||
           fabsf(g_network_sync.weather_longitude - cfg.longitude) > 0.000001f;
}

static void cache_weather_profile(const DeviceConfigSnapshot &cfg)
{
    copy_string(g_network_sync.weather_tz_id, sizeof(g_network_sync.weather_tz_id), cfg.timezone_id);
    copy_string(g_network_sync.weather_location_name, sizeof(g_network_sync.weather_location_name), cfg.location_name);
    g_network_sync.weather_latitude = cfg.latitude;
    g_network_sync.weather_longitude = cfg.longitude;
    copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                cfg.location_name);
}

static void network_sync_reconcile_config(const DeviceConfigSnapshot &cfg)
{
    if (!g_network_sync.ntp_started || strcmp(g_network_sync.active_tz_posix, cfg.timezone_posix) != 0) {
        g_network_sync.snapshot.time_synced = false;
    }

    if (!cfg.weather_configured) {
        clear_weather_snapshot(cfg);
        cache_weather_profile(cfg);
        return;
    }

    if (weather_profile_changed(cfg)) {
        clear_weather_snapshot(cfg);
        cache_weather_profile(cfg);
    }
}

static void network_sync_refresh_config(DeviceConfigSnapshot *cfg)
{
    if (cfg == nullptr) {
        return;
    }
    (void)device_config_get(cfg);
    network_sync_reconcile_config(*cfg);
}

static void network_sync_configure_ntp_if_needed(const DeviceConfigSnapshot &cfg)
{
    if (!g_network_sync.ntp_started || strcmp(g_network_sync.active_tz_posix, cfg.timezone_posix) != 0) {
        configTzTime(cfg.timezone_posix, kNtpServer1, kNtpServer2, kNtpServer3);
        copy_string(g_network_sync.active_tz_posix, sizeof(g_network_sync.active_tz_posix), cfg.timezone_posix);
        g_network_sync.ntp_started = true;
        Serial.printf("[NET] NTP configured tz_posix=%s\n", g_network_sync.active_tz_posix);
    }
}

static bool network_sync_try_time_sync(uint32_t now_ms, const DeviceConfigSnapshot &cfg)
{
    struct tm local_tm;
    DateTime dt;

    network_sync_configure_ntp_if_needed(cfg);
    if (!getLocalTime(&local_tm, 100)) {
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
    Serial.printf("[NET] Time synced: %04u-%02u-%02u %02u:%02u:%02u tz=%s\n",
                  (unsigned)dt.year,
                  (unsigned)dt.month,
                  (unsigned)dt.day,
                  (unsigned)dt.hour,
                  (unsigned)dt.minute,
                  (unsigned)dt.second,
                  cfg.timezone_posix);
    return true;
}

static String network_sync_build_weather_url(const DeviceConfigSnapshot &cfg)
{
    String url;
    char lat_buf[24];
    char lon_buf[24];

    snprintf(lat_buf, sizeof(lat_buf), "%.6f", cfg.latitude);
    snprintf(lon_buf, sizeof(lon_buf), "%.6f", cfg.longitude);

    url.reserve(192);
    url += kWeatherBaseUrl;
    url += "?latitude=";
    url += lat_buf;
    url += "&longitude=";
    url += lon_buf;
    url += "&current=temperature_2m,weather_code";
    url += "&timezone=";
    append_url_encoded(url, cfg.timezone_id);
    url += "&forecast_days=1";
    return url;
}

static bool network_sync_prepare_weather_client(WiFiClientSecure *client)
{
    size_t bundle_len = 0U;
    const char *bundle = nullptr;

    if (client == nullptr) {
        return false;
    }

    bundle = weather_ca_bundle_ptr(&bundle_len);
    if (bundle != nullptr) {
        client->setCACert(bundle);
        update_tls_snapshot(true, true);
        return true;
    }

#if APP_NETWORK_SYNC_TLS_ALLOW_INSECURE
    client->setInsecure();
    update_tls_snapshot(false, false);
    set_weather_status("TLS DEV");
    Serial.println("[NET] Weather TLS running in insecure development mode");
    return true;
#else
    update_tls_snapshot(true, false);
    set_weather_status("CA_MISSING");
    Serial.println("[NET] Weather TLS strict mode blocked because the embedded CA bundle is unavailable");
    return false;
#endif
}

static bool network_sync_fetch_weather(uint32_t now_ms, const DeviceConfigSnapshot &cfg)
{
    WiFiClientSecure client;
    HTTPClient http;
    String payload;
    String url;
    float temp_c = 0.0f;
    uint8_t weather_code = 0U;
    int status_code;

    if (!cfg.weather_configured) {
        clear_weather_snapshot(cfg);
        return false;
    }

    if (!network_sync_prepare_weather_client(&client)) {
        g_network_sync.snapshot.weather_last_attempt_ok = false;
        g_network_sync.snapshot.last_weather_http_status = -1;
        return false;
    }

    url = network_sync_build_weather_url(cfg);
    http.setConnectTimeout(APP_NETWORK_SYNC_WEATHER_CONNECT_TIMEOUT_MS);
    http.setTimeout(APP_NETWORK_SYNC_WEATHER_REQUEST_TIMEOUT_MS);

    if (!http.begin(client, url)) {
        set_weather_status("BEGIN_ERR");
        g_network_sync.snapshot.weather_last_attempt_ok = false;
        g_network_sync.snapshot.last_weather_http_status = -1;
        return false;
    }

    status_code = http.GET();
    g_network_sync.snapshot.last_weather_http_status = status_code;
    if (status_code != HTTP_CODE_OK) {
        Serial.printf("[NET] Weather HTTP error: %d\n", status_code);
        set_weather_status("HTTP_ERR");
        g_network_sync.snapshot.weather_last_attempt_ok = false;
        http.end();
        return false;
    }

    payload = http.getString();
    http.end();

    if (!parse_weather_payload(payload, &temp_c, &weather_code)) {
        Serial.println("[NET] Weather payload parse failed");
        set_weather_status("PARSE_ERR");
        g_network_sync.snapshot.weather_last_attempt_ok = false;
        return false;
    }

    g_network_sync.snapshot.weather_valid = true;
    g_network_sync.snapshot.temperature_tenths_c = (int16_t)lroundf(temp_c * 10.0f);
    g_network_sync.snapshot.weather_code = weather_code;
    copy_string(g_network_sync.snapshot.weather_text,
                sizeof(g_network_sync.snapshot.weather_text),
                network_sync_service_weather_text(g_network_sync.snapshot.weather_code));
    copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                cfg.location_name);
    g_network_sync.snapshot.last_weather_sync_ms = now_ms;
    g_network_sync.snapshot.weather_last_attempt_ok = true;
    set_weather_status("SYNCED");
    cache_weather_profile(cfg);

    Serial.printf("[NET] Weather synced: %s %d.%dC location=%s tls=%s\n",
                  g_network_sync.snapshot.weather_text,
                  (int)(g_network_sync.snapshot.temperature_tenths_c / 10),
                  abs((int)(g_network_sync.snapshot.temperature_tenths_c % 10)),
                  g_network_sync.snapshot.location_name,
                  g_network_sync.snapshot.weather_tls_mode);
    return true;
}
} // namespace

extern "C" bool network_sync_service_init(void)
{
    DeviceConfigSnapshot cfg;

    memset(&g_network_sync, 0, sizeof(g_network_sync));
    device_config_init();
    network_sync_refresh_config(&cfg);
    cache_weather_profile(cfg);
    set_weather_status(cfg.weather_configured ? "PENDING" : "UNSET");
    update_tls_snapshot(true, weather_ca_bundle_ptr(nullptr) != nullptr);
    return true;
}

extern "C" void network_sync_service_tick(uint32_t now_ms)
{
    DeviceConfigSnapshot cfg;
    bool force_refresh;
    bool want_time_sync;
    bool want_weather_sync;

    network_sync_refresh_config(&cfg);
    g_network_sync.snapshot.wifi_connected = web_wifi_connected();
    if (!g_network_sync.snapshot.wifi_connected) {
        return;
    }

    force_refresh = g_network_sync.force_refresh;
    want_time_sync = force_refresh ||
                     !g_network_sync.snapshot.time_synced ||
                     (now_ms - g_network_sync.snapshot.last_time_sync_ms) >= APP_NETWORK_SYNC_TIME_SYNC_PERIOD_MS;
    if (want_time_sync && (force_refresh || (now_ms - g_network_sync.last_time_attempt_ms) >= APP_NETWORK_SYNC_TIME_SYNC_RETRY_MS)) {
        g_network_sync.last_time_attempt_ms = now_ms;
        (void)network_sync_try_time_sync(now_ms, cfg);
    }

    want_weather_sync = cfg.weather_configured &&
                        (force_refresh ||
                         !g_network_sync.snapshot.weather_valid ||
                         (now_ms - g_network_sync.snapshot.last_weather_sync_ms) >= APP_NETWORK_SYNC_WEATHER_PERIOD_MS);
    if (want_weather_sync && (force_refresh || (now_ms - g_network_sync.last_weather_attempt_ms) >= APP_NETWORK_SYNC_WEATHER_RETRY_MS)) {
        g_network_sync.last_weather_attempt_ms = now_ms;
        (void)network_sync_fetch_weather(now_ms, cfg);
    }

    g_network_sync.force_refresh = false;
}

extern "C" void network_sync_service_request_refresh(void)
{
    g_network_sync.force_refresh = true;
}

extern "C" void network_sync_service_on_config_changed(void)
{
    DeviceConfigSnapshot cfg;
    network_sync_refresh_config(&cfg);
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
