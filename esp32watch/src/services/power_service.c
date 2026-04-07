#include "services/power_service.h"
#include "services/display_service.h"
#include "services/diag_service.h"
#include "power.h"
#include "platform_api.h"

static bool g_sleeping;
static WakeReason g_last_wake_reason;
static SleepReason g_last_sleep_reason;
static uint32_t g_last_wake_ms;
static uint32_t g_last_sleep_ms;

void power_service_init(void)
{
    power_init();
    g_sleeping = false;
    g_last_wake_reason = WAKE_REASON_BOOT;
    g_last_sleep_reason = SLEEP_REASON_NONE;
    g_last_wake_ms = platform_time_now_ms();
    g_last_sleep_ms = 0U;
    diag_service_note_wake(g_last_wake_reason);
}

void power_service_apply_settings(const SettingsState *settings)
{
    display_service_apply_settings(settings);
}

void power_service_request_sleep(SleepReason reason)
{
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)reason;
    return;
#else
    if (g_sleeping) {
        return;
    }
    g_last_sleep_reason = reason;
    g_last_sleep_ms = platform_time_now_ms();
    g_sleeping = true;
    diag_service_note_sleep(g_last_sleep_reason);
    display_service_set_sleeping(true);
#endif
}

void power_service_set_sleeping(bool sleeping)
{
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)sleeping;
    g_sleeping = false;
    display_service_set_sleeping(false);
#else
    if (sleeping) {
        power_service_request_sleep(SLEEP_REASON_SERVICE);
    } else {
        power_service_wake_with_reason(WAKE_REASON_SERVICE);
    }
#endif
}

bool power_service_is_sleeping(void)
{
    return g_sleeping;
}

void power_service_wake_with_reason(WakeReason reason)
{
    if (!g_sleeping) {
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
        (void)reason;
        display_service_set_sleeping(false);
        return;
#else
        return;
#endif
    }
    g_last_wake_reason = reason;
    g_last_wake_ms = platform_time_now_ms();
    g_sleeping = false;
    diag_service_note_wake(g_last_wake_reason);
    display_service_set_sleeping(false);
}

void power_service_wake(void)
{
    power_service_wake_with_reason(WAKE_REASON_SERVICE);
}

void power_service_toggle_sleep(void)
{
    if (g_sleeping) {
        power_service_wake_with_reason(WAKE_REASON_MANUAL);
    } else {
        power_service_request_sleep(SLEEP_REASON_MANUAL);
    }
}

WakeReason power_service_get_last_wake_reason(void)
{
    return g_last_wake_reason;
}

SleepReason power_service_get_last_sleep_reason(void)
{
    return g_last_sleep_reason;
}

uint32_t power_service_get_last_wake_ms(void)
{
    return g_last_wake_ms;
}

uint32_t power_service_get_last_sleep_ms(void)
{
    return g_last_sleep_ms;
}

void power_service_idle(bool can_sleep_cpu)
{
    (void)can_sleep_cpu;
}
