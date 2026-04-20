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

void clear_weather_snapshot(const DeviceConfigSnapshot &cfg)
{
    g_network_sync.snapshot.weather_valid = false;
    g_network_sync.snapshot.temperature_tenths_c = 0;
    g_network_sync.snapshot.weather_code = 0U;
    g_network_sync.snapshot.last_weather_sync_ms = 0U;
    g_network_sync.snapshot.weather_last_attempt_ok = false;
    g_network_sync.snapshot.last_weather_http_status = 0U;
    network_sync_copy_string(g_network_sync.snapshot.weather_text,
                sizeof(g_network_sync.snapshot.weather_text),
                cfg.weather_configured ? "SYNC NEEDED" : "UNSET");
    network_sync_copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                cfg.location_name);
    set_weather_status(cfg.weather_configured ? "PENDING" : "UNSET");
}

bool weather_profile_changed(const DeviceConfigSnapshot &cfg)
{
    return strcmp(g_network_sync.weather_tz_id, cfg.timezone_id) != 0 ||
           strcmp(g_network_sync.weather_location_name, cfg.location_name) != 0 ||
           fabsf(g_network_sync.weather_latitude - cfg.latitude) > 0.000001f ||
           fabsf(g_network_sync.weather_longitude - cfg.longitude) > 0.000001f;
}

void cache_weather_profile(const DeviceConfigSnapshot &cfg)
{
    network_sync_copy_string(g_network_sync.weather_tz_id, sizeof(g_network_sync.weather_tz_id), cfg.timezone_id);
    network_sync_copy_string(g_network_sync.weather_location_name, sizeof(g_network_sync.weather_location_name), cfg.location_name);
    g_network_sync.weather_latitude = cfg.latitude;
    g_network_sync.weather_longitude = cfg.longitude;
    network_sync_copy_string(g_network_sync.snapshot.location_name,
                sizeof(g_network_sync.snapshot.location_name),
                cfg.location_name);
}

void network_sync_cancel_weather_work(void)
{
    network_sync_advance_weather_generation();
    if (weather_lock_acquire()) {
        g_network_sync.weather_job_pending = false;
        g_network_sync.weather_job_inflight = false;
        g_network_sync.weather_result_pending = false;
        g_network_sync.weather_job_generation = 0U;
        memset(&g_network_sync.weather_result, 0, sizeof(g_network_sync.weather_result));
        weather_lock_release();
    }
}

bool network_sync_should_accept_weather_result(uint32_t result_generation)
{
    return result_generation != 0U && result_generation == g_network_sync.weather_generation;
}

void network_sync_reconcile_config(const DeviceConfigSnapshot &cfg)
{
    if (!g_network_sync.ntp_started || strcmp(g_network_sync.active_tz_posix, cfg.timezone_posix) != 0) {
        g_network_sync.snapshot.time_synced = false;
    }

    if (!cfg.weather_configured) {
        clear_weather_snapshot(cfg);
        cache_weather_profile(cfg);
        network_sync_cancel_weather_work();
        return;
    }

    if (weather_profile_changed(cfg)) {
        clear_weather_snapshot(cfg);
        cache_weather_profile(cfg);
        network_sync_cancel_weather_work();
    }
}

bool network_sync_refresh_config(DeviceConfigSnapshot *cfg)
{
    if (cfg == nullptr) {
        return false;
    }

    memset(cfg, 0, sizeof(*cfg));
    if (!storage_facade_get_device_config(cfg)) {
        g_network_sync.snapshot.time_synced = false;
        g_network_sync.snapshot.weather_valid = false;
        g_network_sync.snapshot.weather_last_attempt_ok = false;
        g_network_sync.snapshot.last_weather_http_status = 0;
        network_sync_copy_string(g_network_sync.snapshot.location_name,
                    sizeof(g_network_sync.snapshot.location_name),
                    "");
        network_sync_copy_string(g_network_sync.weather_tz_id, sizeof(g_network_sync.weather_tz_id), "");
        network_sync_copy_string(g_network_sync.weather_location_name, sizeof(g_network_sync.weather_location_name), "");
        g_network_sync.weather_latitude = 0.0f;
        g_network_sync.weather_longitude = 0.0f;
        set_weather_status("CFG_ERR");
        return false;
    }

    network_sync_reconcile_config(*cfg);
    g_network_sync.pending_apply_generation = device_config_generation();
    return true;
}
