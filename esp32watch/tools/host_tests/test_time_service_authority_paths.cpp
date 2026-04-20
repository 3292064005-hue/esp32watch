#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
extern "C" {
#include "services/time_service.h"
#include "platform_api.h"
}

extern "C" uint32_t platform_time_now_ms(void)
{
    return host_stub_millis_value;
}

PlatformRtcDevice platform_rtc_main = {0};
static bool g_rtc_supported = false;
static bool g_rtc_persistent = false;
static PlatformStatus g_rtc_status = PLATFORM_STATUS_ERROR;
static uint32_t g_rtc_epoch = 0U;

extern "C" bool platform_rtc_wall_clock_supported(PlatformRtcDevice *device)
{
    (void)device;
    return g_rtc_supported;
}
extern "C" bool platform_rtc_wall_clock_persistent(PlatformRtcDevice *device)
{
    (void)device;
    return g_rtc_persistent;
}
extern "C" PlatformStatus platform_rtc_get_epoch(PlatformRtcDevice *device, uint32_t *out_epoch)
{
    (void)device;
    if (g_rtc_status != PLATFORM_STATUS_OK || out_epoch == NULL) {
        return g_rtc_status;
    }
    *out_epoch = g_rtc_epoch;
    return PLATFORM_STATUS_OK;
}
extern "C" PlatformStatus platform_rtc_set_epoch(PlatformRtcDevice *device, uint32_t epoch)
{
    (void)device;
    g_rtc_epoch = epoch;
    g_rtc_status = PLATFORM_STATUS_OK;
    return PLATFORM_STATUS_OK;
}

static void reset_rtc(bool supported, bool persistent, PlatformStatus status, uint32_t epoch)
{
    g_rtc_supported = supported;
    g_rtc_persistent = persistent;
    g_rtc_status = status;
    g_rtc_epoch = epoch;
    host_stub_millis_value = 0U;
}

int main()
{
    TimeSourceSnapshot source = {};
    DateTime now = {};
    DateTime host_dt = {2026, 4, 19, 0, 9, 45, 30};
    uint32_t host_epoch = time_service_datetime_to_epoch(&host_dt);

    reset_rtc(true, true, PLATFORM_STATUS_OK, 1776591930U);
    assert(time_service_init());
    assert(time_service_get_datetime_snapshot(&now, &source));
    assert(source.source == TIME_SOURCE_DEVICE_CLOCK);
    assert(source.authority == TIME_AUTHORITY_HARDWARE);
    assert(source.authoritative);
    assert(time_service_init_status() == TIME_INIT_STATUS_READY);
    assert(strcmp(time_service_init_reason(), "DEVICE_CLOCK") == 0);
    assert(time_service_authority_level() == TIME_AUTHORITY_HARDWARE);

    host_stub_millis_value = 2000U;
    assert(time_service_set_epoch_from_source(host_epoch, TIME_SOURCE_NETWORK_SYNC));
    assert(time_service_get_source_snapshot(&source));
    assert(source.source == TIME_SOURCE_NETWORK_SYNC);
    assert(source.authority == TIME_AUTHORITY_NETWORK);
    assert(source.confidence == TIME_CONFIDENCE_HIGH);
    assert(strcmp(time_service_init_reason(), "NETWORK_SYNC") == 0);

    host_stub_millis_value = 8000U;
    assert(time_service_try_set_datetime(&host_dt, TIME_SOURCE_HOST_SYNC));
    assert(time_service_get_source_snapshot(&source));
    assert(source.source == TIME_SOURCE_HOST_SYNC);
    assert(source.authority == TIME_AUTHORITY_HOST);
    assert(source.confidence == TIME_CONFIDENCE_MEDIUM);
    assert(source.source_age_ms == 0U);
    assert(strcmp(time_service_init_reason(), "HOST_SYNC") == 0);
    host_stub_millis_value = 20000U;
    assert(time_service_get_source_snapshot(&source));
    assert(source.source_age_ms == 12000U);

    time_service_note_recovery_epoch(host_epoch, true);
    assert(time_service_get_source_snapshot(&source));
    assert(source.source == TIME_SOURCE_RECOVERY);
    assert(source.authority == TIME_AUTHORITY_RECOVERY);
    assert(strcmp(time_service_init_reason(), "RECOVERY_PERSISTED") == 0);

    puts("[OK] time service authority path coverage passed");
    return 0;
}
