#include <Arduino.h>
#include <Preferences.h>
extern "C" {
#include "services/time_service.h"
#include "platform_api.h"
}

static Preferences g_time_prefs;
static TimeSourceSnapshot g_time_source_snapshot;
static uint32_t g_epoch_base;
static uint32_t g_base_ms;
static bool g_time_loaded;
static const uint32_t kEpochFloor = 1704067200UL;

static bool time_is_leap(uint16_t year)
{
    return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

static uint16_t time_days_before_month(uint16_t year, uint8_t month)
{
    static const uint16_t acc[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    uint16_t days = acc[(month - 1U) % 12U];
    if (month > 2U && time_is_leap(year)) days++;
    return days;
}

static uint8_t time_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t dim_tbl[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t dim = dim_tbl[(month - 1U) % 12U];
    if (month == 2U && time_is_leap(year)) dim = 29U;
    return dim;
}

static void time_save_epoch(uint32_t epoch)
{
    if (!g_time_prefs.begin("watch_rtc", false)) {
        return;
    }
    g_time_prefs.putULong("epoch", epoch);
    g_time_prefs.end();
}

static void time_load_epoch(void)
{
    if (g_time_loaded) {
        return;
    }
    if (!g_time_prefs.begin("watch_rtc", true)) {
        g_epoch_base = kEpochFloor;
        g_base_ms = millis();
        g_time_loaded = true;
        return;
    }
    g_epoch_base = g_time_prefs.getULong("epoch", kEpochFloor);
    g_time_prefs.end();
    if (g_epoch_base < kEpochFloor) {
        g_epoch_base = kEpochFloor;
    }
    g_base_ms = millis();
    g_time_loaded = true;
}

extern "C" void time_service_init(void)
{
    time_load_epoch();
    g_time_source_snapshot.source = TIME_SOURCE_RTC;
    g_time_source_snapshot.epoch = time_service_get_epoch();
    g_time_source_snapshot.valid = time_service_is_reasonable_epoch(g_time_source_snapshot.epoch);
    g_time_source_snapshot.updated_at_ms = platform_time_now_ms();
}

extern "C" bool time_service_is_datetime_valid(const DateTime *dt)
{
    if (dt == NULL) return false;
    if (dt->year < 1970U || dt->year > 2099U) return false;
    if (dt->month < 1U || dt->month > 12U) return false;
    if (dt->day < 1U || dt->day > time_days_in_month(dt->year, dt->month)) return false;
    if (dt->hour > 23U || dt->minute > 59U || dt->second > 59U) return false;
    return true;
}

extern "C" uint32_t time_service_datetime_to_epoch(const DateTime *dt)
{
    uint32_t days = 0U;
    if (!time_service_is_datetime_valid(dt)) return 0U;
    for (uint16_t y = 1970U; y < dt->year; ++y) {
        days += time_is_leap(y) ? 366U : 365U;
    }
    days += time_days_before_month(dt->year, dt->month);
    days += (uint32_t)(dt->day - 1U);
    return days * 86400UL + (uint32_t)dt->hour * 3600UL + (uint32_t)dt->minute * 60UL + dt->second;
}

extern "C" void time_service_epoch_to_datetime(uint32_t epoch, DateTime *out)
{
    uint32_t days = epoch / 86400UL;
    uint32_t sec = epoch % 86400UL;
    uint16_t year = 1970U;
    uint8_t month = 1U;

    if (out == NULL) return;
    out->hour = (uint8_t)(sec / 3600UL);
    sec %= 3600UL;
    out->minute = (uint8_t)(sec / 60UL);
    out->second = (uint8_t)(sec % 60UL);
    while (1) {
        uint16_t d = time_is_leap(year) ? 366U : 365U;
        if (days < d) break;
        days -= d;
        year++;
    }
    while (month <= 12U) {
        uint8_t dim = time_days_in_month(year, month);
        if (days < dim) break;
        days -= dim;
        month++;
    }
    out->year = year;
    out->month = month;
    out->day = (uint8_t)(days + 1U);
    out->weekday = (uint8_t)(((epoch / 86400UL) + 4U) % 7U);
}

extern "C" uint32_t time_service_get_epoch(void)
{
    time_load_epoch();
    uint32_t elapsed_sec = (platform_time_now_ms() - g_base_ms) / 1000UL;
    uint32_t epoch = g_epoch_base + elapsed_sec;
    if (g_time_source_snapshot.source == TIME_SOURCE_RTC) {
        g_time_source_snapshot.epoch = epoch;
        g_time_source_snapshot.valid = time_service_is_reasonable_epoch(epoch);
    }
    return epoch;
}

extern "C" void time_service_refresh(DateTime *out)
{
    if (out == NULL) return;
    time_service_epoch_to_datetime(time_service_get_epoch(), out);
}

extern "C" bool time_service_set_epoch_from_source(uint32_t epoch, TimeSourceType source)
{
    if (!time_service_is_reasonable_epoch(epoch)) return false;
    g_epoch_base = epoch;
    g_base_ms = platform_time_now_ms();
    time_save_epoch(epoch);
    g_time_source_snapshot.source = source;
    g_time_source_snapshot.epoch = epoch;
    g_time_source_snapshot.valid = true;
    g_time_source_snapshot.updated_at_ms = platform_time_now_ms();
    return true;
}

extern "C" bool time_service_try_set_datetime(const DateTime *dt, TimeSourceType source)
{
    if (!time_service_is_datetime_valid(dt)) return false;
    return time_service_set_epoch_from_source(time_service_datetime_to_epoch(dt), source);
}

extern "C" void time_service_set_datetime(const DateTime *dt)
{
    (void)time_service_try_set_datetime(dt, TIME_SOURCE_HOST_SYNC);
}

extern "C" void time_service_note_recovery_epoch(uint32_t epoch, bool valid)
{
    g_time_source_snapshot.source = TIME_SOURCE_RECOVERY;
    g_time_source_snapshot.epoch = epoch;
    g_time_source_snapshot.valid = valid;
    g_time_source_snapshot.updated_at_ms = platform_time_now_ms();
}

extern "C" bool time_service_get_source_snapshot(TimeSourceSnapshot *out)
{
    if (out == NULL) return false;
    if (g_time_source_snapshot.source == TIME_SOURCE_RTC) {
        g_time_source_snapshot.epoch = time_service_get_epoch();
        g_time_source_snapshot.valid = time_service_is_reasonable_epoch(g_time_source_snapshot.epoch);
    }
    *out = g_time_source_snapshot;
    return true;
}

extern "C" const char *time_service_source_name(TimeSourceType source)
{
    switch (source) {
        case TIME_SOURCE_RTC: return "RTC";
        case TIME_SOURCE_HOST_SYNC: return "HOST";
        case TIME_SOURCE_NETWORK_SYNC: return "NTP";
        case TIME_SOURCE_COMPANION_SYNC: return "COMPANION";
        case TIME_SOURCE_RECOVERY: return "RECOVERY";
        default: return "UNKNOWN";
    }
}

extern "C" uint16_t time_service_day_id_from_epoch(uint32_t epoch)
{
    return (uint16_t)(epoch / 86400UL);
}

extern "C" bool time_service_is_reasonable_epoch(uint32_t epoch)
{
    return epoch >= kEpochFloor;
}

