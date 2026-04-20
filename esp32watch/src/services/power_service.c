#include "services/power_service.h"
#include "services/display_service.h"
#include "services/diag_service.h"
#include "services/runtime_event_service.h"
#include "services/runtime_reload_coordinator.h"
#include "power.h"
#include "esp32_port_config.h"
#include "platform_api.h"

static bool g_sleeping;
static WakeReason g_last_wake_reason;
static SleepReason g_last_sleep_reason;
static uint32_t g_last_wake_ms;
static uint32_t g_last_sleep_ms;
static uint32_t g_last_idle_budget_ms;
static uint32_t g_idle_sleep_attempt_count;
static uint32_t g_idle_sleep_reject_count;
static bool g_last_idle_sleep_ok;
static bool g_power_service_initialized;
static uint32_t g_last_applied_generation;

static void power_service_note_applied_generation(void)
{
    g_last_applied_generation = device_config_generation();
}

static bool power_service_handle_runtime_event(RuntimeServiceEvent event, void *ctx)
{
    ModelDomainState domain_state = {0};
    (void)ctx;
    if (event != RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED) {
        return true;
    }
    if (!g_power_service_initialized || model_get_domain_state(&domain_state) == NULL) {
        return false;
    }
    power_service_apply_settings(&domain_state.settings);
    power_service_note_applied_generation();
    return true;
}

static uint32_t power_service_idle_budget_ms(bool can_sleep_cpu)
{
    uint32_t budget_ms;

    if (!can_sleep_cpu) {
        return 0U;
    }

    budget_ms = g_sleeping ? ESP32_IDLE_LIGHT_SLEEP_UI_SLEEP_MS : ESP32_IDLE_LIGHT_SLEEP_ACTIVE_MS;
    if (budget_ms < ESP32_IDLE_LIGHT_SLEEP_MIN_MS) {
        budget_ms = ESP32_IDLE_LIGHT_SLEEP_MIN_MS;
    }
    if (budget_ms > ESP32_IDLE_LIGHT_SLEEP_MAX_MS) {
        budget_ms = ESP32_IDLE_LIGHT_SLEEP_MAX_MS;
    }
    return budget_ms;
}

void power_service_init(void)
{
    RuntimeEventSubscription subscription = {
        .handler = power_service_handle_runtime_event,
        .ctx = NULL,
        .name = "power_service",
        .priority = 0,
        .critical = true,
        .event_mask = runtime_event_service_event_mask(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED),
        .domain_mask = RUNTIME_RELOAD_DOMAIN_POWER,
    };

    power_init();
    g_sleeping = false;
    g_last_wake_reason = WAKE_REASON_BOOT;
    g_last_sleep_reason = SLEEP_REASON_NONE;
    g_last_wake_ms = platform_time_now_ms();
    g_last_sleep_ms = 0U;
    g_last_idle_budget_ms = 0U;
    g_idle_sleep_attempt_count = 0U;
    g_idle_sleep_reject_count = 0U;
    g_last_idle_sleep_ok = true;
    g_power_service_initialized = runtime_event_service_register_ex(&subscription);
    if (g_power_service_initialized) {
        power_service_note_applied_generation();
    }
    diag_service_note_wake(g_last_wake_reason);
}

void power_service_apply_settings(const SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
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
    (void)runtime_event_service_publish_notify(RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED);
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
    (void)runtime_event_service_publish_notify(RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED);
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

uint32_t power_service_get_last_idle_budget_ms(void)
{
    return g_last_idle_budget_ms;
}

bool power_service_last_idle_sleep_ok(void)
{
    return g_last_idle_sleep_ok;
}

uint32_t power_service_get_idle_sleep_attempt_count(void)
{
    return g_idle_sleep_attempt_count;
}

uint32_t power_service_get_idle_sleep_reject_count(void)
{
    return g_idle_sleep_reject_count;
}

void power_service_idle(bool can_sleep_cpu)
{
    uint32_t idle_budget_ms;

#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)can_sleep_cpu;
    return;
#else
    if (ESP32_IDLE_LIGHT_SLEEP_ENABLED == 0) {
        (void)can_sleep_cpu;
        return;
    }

    idle_budget_ms = power_service_idle_budget_ms(can_sleep_cpu);
    g_last_idle_budget_ms = idle_budget_ms;
    if (idle_budget_ms == 0U) {
        g_last_idle_sleep_ok = false;
        return;
    }

    ++g_idle_sleep_attempt_count;
    g_last_idle_sleep_ok = platform_light_sleep_for(idle_budget_ms);
    if (!g_last_idle_sleep_ok) {
        ++g_idle_sleep_reject_count;
    }
#endif
}

uint32_t power_service_last_applied_generation(void)
{
    return g_last_applied_generation;
}

bool power_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_power_service_initialized && generation != 0U && g_last_applied_generation == generation;
}
