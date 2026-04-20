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

#if defined(HOST_RUNTIME_TEST)
const char *const kWeatherCaBundleStart = "";
const char *const kWeatherCaBundleEnd = "";
#else
extern const uint8_t _binary_data_certs_weather_ca_bundle_pem_start[] asm("_binary_data_certs_weather_ca_bundle_pem_start");
extern const uint8_t _binary_data_certs_weather_ca_bundle_pem_end[] asm("_binary_data_certs_weather_ca_bundle_pem_end");
const char *const kWeatherCaBundleStart = reinterpret_cast<const char *>(_binary_data_certs_weather_ca_bundle_pem_start);
const char *const kWeatherCaBundleEnd = reinterpret_cast<const char *>(_binary_data_certs_weather_ca_bundle_pem_end);
#endif

bool weather_lock_acquire(void)
{
    if (g_network_sync.weather_lock == nullptr) {
        return false;
    }
    return xSemaphoreTake(g_network_sync.weather_lock, portMAX_DELAY) == pdTRUE;
}

void weather_lock_release(void)
{
    if (g_network_sync.weather_lock != nullptr) {
        (void)xSemaphoreGive(g_network_sync.weather_lock);
    }
}

void network_sync_copy_string(char *dst, size_t dst_size, const char *src)
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

void set_weather_status(const char *status)
{
    network_sync_copy_string(g_network_sync.snapshot.weather_status,
                sizeof(g_network_sync.snapshot.weather_status),
                status);
}

void update_tls_snapshot(bool strict_mode, bool ca_loaded)
{
    g_network_sync.snapshot.weather_ca_loaded = ca_loaded;
    g_network_sync.snapshot.weather_tls_verified = strict_mode && ca_loaded;
    network_sync_copy_string(g_network_sync.snapshot.weather_tls_mode,
                sizeof(g_network_sync.snapshot.weather_tls_mode),
                strict_mode ? "STRICT" : "INSECURE");
}

void set_result_tls_snapshot(WeatherFetchResult *result, bool strict_mode, bool ca_loaded)
{
    if (result == nullptr) {
        return;
    }
    result->tls_verified = strict_mode && ca_loaded;
    result->ca_loaded = ca_loaded;
    network_sync_copy_string(result->tls_mode, sizeof(result->tls_mode), strict_mode ? "STRICT" : "INSECURE");
}

const char *weather_ca_bundle_ptr(size_t *out_len)
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

void note_applied_generation(void)
{
    g_network_sync.last_applied_generation = device_config_generation();
}

void network_sync_advance_weather_generation(void)
{
    if (g_network_sync.weather_generation == UINT32_MAX) {
        g_network_sync.weather_generation = 1U;
        return;
    }
    ++g_network_sync.weather_generation;
    if (g_network_sync.weather_generation == 0U) {
        g_network_sync.weather_generation = 1U;
    }
}

bool apply_config_changed(void)
{
    DeviceConfigSnapshot cfg;

    if (!g_network_sync.initialized) {
        return false;
    }
    if (!network_sync_refresh_config(&cfg)) {
        return false;
    }
    network_sync_configure_ntp_if_needed(cfg);
    cache_weather_profile(cfg);
    note_applied_generation();
    g_network_sync.pending_apply_generation = g_network_sync.last_applied_generation;
    g_network_sync.force_refresh = true;
    return true;
}

bool handle_runtime_event(RuntimeServiceEvent event, void *ctx)
{
    (void)ctx;
    if (event != RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED) {
        return true;
    }
    return apply_config_changed();
}

void network_sync_service_cleanup_init_failure(void)
{
    if (g_network_sync.weather_worker != nullptr) {
        vTaskDelete(g_network_sync.weather_worker);
        g_network_sync.weather_worker = nullptr;
        g_network_sync.weather_worker_ready = false;
    }
    if (g_network_sync.weather_lock != nullptr) {
        vSemaphoreDelete(g_network_sync.weather_lock);
        g_network_sync.weather_lock = nullptr;
    }
    (void)runtime_event_service_unregister(handle_runtime_event, nullptr);
}
