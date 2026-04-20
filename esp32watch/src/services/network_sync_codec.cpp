#include "services/network_sync_codec.h"
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>

bool network_sync_parse_weather_payload(const String &payload, float *out_temp_c, uint8_t *out_weather_code)
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

void network_sync_append_url_encoded(String &dst, const char *src)
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

String network_sync_build_weather_url(const DeviceConfigSnapshot &cfg, const char *base_url)
{
    String url;
    char lat_buf[24];
    char lon_buf[24];

    snprintf(lat_buf, sizeof(lat_buf), "%.6f", cfg.latitude);
    snprintf(lon_buf, sizeof(lon_buf), "%.6f", cfg.longitude);

    url.reserve(192);
    url += (base_url != nullptr) ? base_url : "";
    url += "?latitude=";
    url += lat_buf;
    url += "&longitude=";
    url += lon_buf;
    url += "&current=temperature_2m,weather_code";
    url += "&timezone=";
    network_sync_append_url_encoded(url, cfg.timezone_id);
    url += "&forecast_days=1";
    return url;
}
