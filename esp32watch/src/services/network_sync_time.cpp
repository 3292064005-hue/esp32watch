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

void network_sync_configure_ntp_if_needed(const DeviceConfigSnapshot &cfg)
{
    if (!g_network_sync.ntp_started || strcmp(g_network_sync.active_tz_posix, cfg.timezone_posix) != 0) {
        configTzTime(cfg.timezone_posix, kNtpServer1, kNtpServer2, kNtpServer3);
        network_sync_copy_string(g_network_sync.active_tz_posix, sizeof(g_network_sync.active_tz_posix), cfg.timezone_posix);
        g_network_sync.ntp_started = true;
        Serial.printf("[NET] NTP configured tz_posix=%s\n", g_network_sync.active_tz_posix);
    }
}

bool network_sync_try_time_sync(uint32_t now_ms, const DeviceConfigSnapshot &cfg)
{
    struct tm local_tm;
    DateTime dt;

    network_sync_configure_ntp_if_needed(cfg);
    if (!getLocalTime(&local_tm, APP_NETWORK_SYNC_TIME_SYNC_WAIT_MS)) {
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
