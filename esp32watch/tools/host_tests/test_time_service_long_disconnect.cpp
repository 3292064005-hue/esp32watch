#include <assert.h>
#include <stdio.h>

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
extern "C" bool platform_rtc_wall_clock_supported(PlatformRtcDevice *device)
{
    (void)device;
    return false;
}
extern "C" bool platform_rtc_wall_clock_persistent(PlatformRtcDevice *device)
{
    (void)device;
    return false;
}
extern "C" PlatformStatus platform_rtc_get_epoch(PlatformRtcDevice *device, uint32_t *out_epoch)
{
    (void)device;
    (void)out_epoch;
    return PLATFORM_STATUS_ERROR;
}
extern "C" PlatformStatus platform_rtc_set_epoch(PlatformRtcDevice *device, uint32_t epoch)
{
    (void)device;
    (void)epoch;
    return PLATFORM_STATUS_ERROR;
}

static DateTime make_dt(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    DateTime dt = {};
    dt.year = year;
    dt.month = month;
    dt.day = day;
    dt.hour = hour;
    dt.minute = minute;
    dt.second = second;
    return dt;
}

int main()
{
    DateTime current = {};
    TimeSourceSnapshot source = {};
    DateTime network_dt = make_dt(2026U, 4U, 19U, 8U, 0U, 0U);

    host_stub_millis_value = 0U;
    assert(time_service_init());
    assert(time_service_try_set_datetime(&network_dt, TIME_SOURCE_NETWORK_SYNC));
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source == TIME_SOURCE_NETWORK_SYNC);
    assert(source.authority == TIME_AUTHORITY_NETWORK);
    assert(source.source_age_ms == 0U);

    host_stub_millis_value = 3U * 24U * 60U * 60U * 1000U;
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source == TIME_SOURCE_NETWORK_SYNC);
    assert(source.authority == TIME_AUTHORITY_NETWORK);
    assert(source.source_age_ms == host_stub_millis_value);
    assert(current.day >= 22U);

    host_stub_millis_value += 5000U;
    network_dt.day = current.day;
    network_dt.hour = current.hour;
    network_dt.minute = current.minute;
    network_dt.second = current.second;
    assert(time_service_try_set_datetime(&network_dt, TIME_SOURCE_NETWORK_SYNC));
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source == TIME_SOURCE_NETWORK_SYNC);
    assert(source.authority == TIME_AUTHORITY_NETWORK);
    assert(source.source_age_ms == 0U);

    puts("[OK] time service long disconnect + resync coverage passed");
    return 0;
}
