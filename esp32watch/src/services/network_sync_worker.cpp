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

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif

void network_sync_weather_worker(void *arg)
{
    (void)arg;
    g_network_sync.weather_worker_ready = true;
    for (;;) {
        DeviceConfigSnapshot cfg = {};
        uint32_t generation = 0U;
        bool should_fetch = false;
        if (weather_lock_acquire()) {
            if (g_network_sync.weather_job_pending) {
                cfg = g_network_sync.weather_job_cfg;
                generation = g_network_sync.weather_job_generation;
                g_network_sync.weather_job_pending = false;
                g_network_sync.weather_job_inflight = true;
                should_fetch = true;
            }
            weather_lock_release();
        }
        if (should_fetch) {
            WeatherFetchResult result = {};
            const uint32_t now_ms = millis();
            (void)network_sync_fetch_weather_blocking(now_ms, &cfg, generation, &result);
            if (weather_lock_acquire()) {
                g_network_sync.weather_result = result;
                g_network_sync.weather_result_pending = true;
                g_network_sync.weather_job_inflight = false;
                weather_lock_release();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kWeatherWorkerPollDelayMs));
    }
}

bool network_sync_start_weather_worker(void)
{
    if (g_network_sync.weather_worker != nullptr) {
        return true;
    }
#if defined(HOST_RUNTIME_TEST)
    g_network_sync.weather_worker = (TaskHandle_t)0x1;
    g_network_sync.weather_worker_ready = true;
    return true;
#else
    BaseType_t task_ok = xTaskCreatePinnedToCore(network_sync_weather_worker,
                                                 "weather_sync",
                                                 kWeatherWorkerStackDepth,
                                                 nullptr,
                                                 kWeatherWorkerPriority,
                                                 &g_network_sync.weather_worker,
                                                 0);
    return task_ok == pdPASS && g_network_sync.weather_worker != nullptr;
#endif
}
