#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
extern "C" {
#include "services/time_service.h"
}

extern "C" uint32_t platform_time_now_ms(void)
{
    return host_stub_millis_value;
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
    DateTime invalid = make_dt(1969U, 12U, 31U, 23U, 59U, 59U);
    DateTime valid = make_dt(2026U, 4U, 14U, 9U, 30U, 15U);

    host_stub_millis_value = 0U;
    assert(time_service_init());
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source == TIME_SOURCE_RECOVERY);
    assert(source.valid);
    assert(source.confidence == TIME_CONFIDENCE_LOW);

    assert(!time_service_try_set_datetime(&invalid, TIME_SOURCE_HOST_SYNC));
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source == TIME_SOURCE_RECOVERY);

    assert(time_service_try_set_datetime(&valid, TIME_SOURCE_HOST_SYNC));
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source == TIME_SOURCE_HOST_SYNC);
    assert(source.valid);
    assert(source.confidence == TIME_CONFIDENCE_MEDIUM);
    assert(!source.authoritative);
    assert(current.year == 2026U);
    assert(current.month == 4U);
    assert(current.day == 14U);

    host_stub_millis_value = 5000U;
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source_age_ms == 5000U);

    time_service_note_recovery_epoch(123U, false);
    assert(time_service_get_datetime_snapshot(&current, &source));
    assert(source.source == TIME_SOURCE_RECOVERY);
    assert(!source.valid);
    assert(source.confidence == TIME_CONFIDENCE_NONE);

    puts("[OK] time_service fault behavior check passed");
    return 0;
}
