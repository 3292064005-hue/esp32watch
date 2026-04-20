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

NetworkSyncState g_network_sync = {};

extern "C" bool network_sync_service_init(void)
{
    DeviceConfigSnapshot cfg;

    memset(&g_network_sync, 0, sizeof(g_network_sync));
    g_network_sync.weather_generation = 1U;
    RuntimeEventSubscription subscription = {
        .handler = handle_runtime_event,
        .ctx = nullptr,
        .name = "network_sync",
        .priority = 10,
        .critical = false,
        .event_mask = runtime_event_service_event_mask(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED),
        .domain_mask = RUNTIME_RELOAD_DOMAIN_NETWORK,
    };
    if (!runtime_event_service_register_ex(&subscription)) {
        return false;
    }
    g_network_sync.weather_lock = xSemaphoreCreateMutex();
    if (g_network_sync.weather_lock == nullptr) {
        (void)runtime_event_service_unregister(handle_runtime_event, nullptr);
        return false;
    }
    if (!network_sync_start_weather_worker()) {
        network_sync_service_cleanup_init_failure();
        return false;
    }
    g_network_sync.initialized = true;
    if (!network_sync_refresh_config(&cfg)) {
        g_network_sync.initialized = false;
        network_sync_service_cleanup_init_failure();
        return false;
    }
    network_sync_configure_ntp_if_needed(cfg);
    cache_weather_profile(cfg);
    note_applied_generation();
    g_network_sync.pending_apply_generation = g_network_sync.last_applied_generation;
    set_weather_status(cfg.weather_configured ? "PENDING" : "UNSET");
    update_tls_snapshot(true, weather_ca_bundle_ptr(nullptr) != nullptr);
    return true;
}

/**
 * @brief Advance the network synchronization service without blocking the cooperative runtime loop.
 *
 * Performs bounded work only: opportunistic NTP refresh, weather-worker result consumption,
 * and at most one weather job dispatch. Blocking HTTPS weather I/O is executed exclusively by
 * the dedicated worker task created during service initialization.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return void
 * @throws None.
 * @boundary_behavior Returns early when configuration is unavailable or Wi-Fi is offline, while retaining the last successful weather snapshot for diagnostics.
 */
extern "C" void network_sync_service_tick(uint32_t now_ms)
{
    DeviceConfigSnapshot cfg;
    bool force_refresh;
    bool want_time_sync;
    bool want_weather_sync;

    if (!network_sync_refresh_config(&cfg)) {
        return;
    }
    g_network_sync.snapshot.wifi_connected = web_wifi_connected();
    network_sync_apply_weather_result();
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
    bool weather_job_inflight = false;
    bool weather_job_pending = false;
    if (weather_lock_acquire()) {
        weather_job_inflight = g_network_sync.weather_job_inflight;
        weather_job_pending = g_network_sync.weather_job_pending;
        weather_lock_release();
    }
    if (weather_job_inflight) {
        set_weather_status("SYNCING");
    } else if (want_weather_sync && !weather_job_pending &&
               (force_refresh || (now_ms - g_network_sync.last_weather_attempt_ms) >= APP_NETWORK_SYNC_WEATHER_RETRY_MS)) {
        g_network_sync.last_weather_attempt_ms = now_ms;
        network_sync_queue_weather_job(cfg);
    }

    note_applied_generation();
    g_network_sync.force_refresh = false;
}






