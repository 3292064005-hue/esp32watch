#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

extern "C" {
#include "services/storage_facade.h"
#include "services/network_sync_service.h"
#include "services/network_sync_build_config.h"
#include "services/time_service.h"
#include "services/runtime_event_service.h"
#include "services/runtime_reload_coordinator.h"
#include "web/web_wifi.h"
}
#include "services/network_sync_codec.h"
#include "services/network_sync_internal.hpp"

bool network_sync_prepare_weather_client(WiFiClientSecure *client, WeatherFetchResult *result)
{
    size_t bundle_len = 0U;
    const char *bundle = nullptr;

    if (client == nullptr || result == nullptr) {
        return false;
    }

    bundle = weather_ca_bundle_ptr(&bundle_len);
    if (bundle != nullptr) {
        client->setCACert(bundle);
        set_result_tls_snapshot(result, true, true);
        return true;
    }

#if APP_NETWORK_SYNC_TLS_ALLOW_INSECURE
    client->setInsecure();
    set_result_tls_snapshot(result, false, false);
    network_sync_copy_string(result->status, sizeof(result->status), "TLS DEV");
    return true;
#else
    set_result_tls_snapshot(result, true, false);
    network_sync_copy_string(result->status, sizeof(result->status), "CA_MISSING");
    return false;
#endif
}

bool network_sync_fetch_weather_blocking(uint32_t now_ms,
                                         const DeviceConfigSnapshot *cfg,
                                         uint32_t generation,
                                         WeatherFetchResult *result)
{
    WiFiClientSecure client;
    HTTPClient http;
    String payload;
    String url;
    float temp_c = 0.0f;
    uint8_t weather_code = 0U;
    int status_code;

    if (cfg == nullptr || result == nullptr) {
        return false;
    }

    memset(result, 0, sizeof(*result));
    result->generation = generation;
    result->http_status = 0;
    network_sync_copy_string(result->location_name, sizeof(result->location_name), cfg->location_name);

    if (!cfg->weather_configured) {
        network_sync_copy_string(result->weather_text, sizeof(result->weather_text), "UNSET");
        network_sync_copy_string(result->status, sizeof(result->status), "UNSET");
        result->completed = true;
        return false;
    }

    if (!network_sync_prepare_weather_client(&client, result)) {
        result->last_attempt_ok = false;
        result->http_status = -1;
        result->completed = true;
        return false;
    }

    url = network_sync_build_weather_url(*cfg, kWeatherBaseUrl);
    http.setConnectTimeout(APP_NETWORK_SYNC_WEATHER_CONNECT_TIMEOUT_MS);
    http.setTimeout(APP_NETWORK_SYNC_WEATHER_REQUEST_TIMEOUT_MS);

    if (!http.begin(client, url)) {
        network_sync_copy_string(result->status, sizeof(result->status), "BEGIN_ERR");
        result->last_attempt_ok = false;
        result->http_status = -1;
        result->completed = true;
        return false;
    }

    status_code = http.GET();
    result->http_status = status_code;
    if (status_code != HTTP_CODE_OK) {
        network_sync_copy_string(result->status, sizeof(result->status), "HTTP_ERR");
        result->last_attempt_ok = false;
        http.end();
        result->completed = true;
        return false;
    }

    payload = http.getString();
    http.end();

    if (!network_sync_parse_weather_payload(payload, &temp_c, &weather_code)) {
        network_sync_copy_string(result->status, sizeof(result->status), "PARSE_ERR");
        result->last_attempt_ok = false;
        result->completed = true;
        return false;
    }

    result->success = true;
    result->last_attempt_ok = true;
    result->temperature_tenths_c = (int16_t)lroundf(temp_c * 10.0f);
    result->weather_code = weather_code;
    result->completed_at_ms = now_ms;
    network_sync_copy_string(result->weather_text, sizeof(result->weather_text), network_sync_service_weather_text(weather_code));
    network_sync_copy_string(result->status, sizeof(result->status), "SYNCED");
    result->completed = true;
    return true;
}

void network_sync_apply_weather_result(void)
{
    WeatherFetchResult result = {};
    bool had_result = false;
    if (weather_lock_acquire()) {
        if (g_network_sync.weather_result_pending) {
            result = g_network_sync.weather_result;
            g_network_sync.weather_result_pending = false;
            had_result = true;
        }
        weather_lock_release();
    }
    if (!had_result) {
        return;
    }
    if (!network_sync_should_accept_weather_result(result.generation)) {
        return;
    }
    g_network_sync.snapshot.weather_last_attempt_ok = result.last_attempt_ok;
    g_network_sync.snapshot.last_weather_http_status = result.http_status;
    network_sync_copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                result.location_name);
    network_sync_copy_string(g_network_sync.snapshot.weather_tls_mode,
                sizeof(g_network_sync.snapshot.weather_tls_mode),
                result.tls_mode);
    g_network_sync.snapshot.weather_tls_verified = result.tls_verified;
    g_network_sync.snapshot.weather_ca_loaded = result.ca_loaded;
    if (result.success) {
        g_network_sync.snapshot.weather_valid = true;
        g_network_sync.snapshot.temperature_tenths_c = result.temperature_tenths_c;
        g_network_sync.snapshot.weather_code = result.weather_code;
        g_network_sync.snapshot.last_weather_sync_ms = result.completed_at_ms;
        network_sync_copy_string(g_network_sync.snapshot.weather_text,
                    sizeof(g_network_sync.snapshot.weather_text),
                    result.weather_text);
        set_weather_status("SYNCED");
    } else {
        g_network_sync.snapshot.weather_valid = false;
        network_sync_copy_string(g_network_sync.snapshot.weather_text,
                    sizeof(g_network_sync.snapshot.weather_text),
                    result.status);
        set_weather_status(result.status);
    }
}

void network_sync_queue_weather_job(const DeviceConfigSnapshot &cfg)
{
    if (!weather_lock_acquire()) {
        return;
    }
    g_network_sync.weather_job_cfg = cfg;
    g_network_sync.weather_job_generation = g_network_sync.weather_generation;
    g_network_sync.weather_job_pending = true;
    g_network_sync.weather_result_pending = false;
    weather_lock_release();
    set_weather_status("QUEUED");
}
