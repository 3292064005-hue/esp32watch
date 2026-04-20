#include "services/runtime_reload_coordinator.h"
#include "services/runtime_event_service.h"
#include "services/device_config.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool g_event_should_succeed = true;
static bool g_wifi_verify_ok = true;
static bool g_network_verify_ok = true;
static bool g_display_verify_ok = true;
static bool g_power_verify_ok = true;
static bool g_sensor_verify_ok = true;
static bool g_config_ok = true;
static uint32_t g_config_generation = 7U;
static DeviceConfigSnapshot g_cfg = {
    .version = DEVICE_CONFIG_VERSION,
    .wifi_configured = true,
    .weather_configured = true,
    .api_token_configured = false,
    .wifi_ssid = "watch-net",
    .timezone_posix = "UTC0",
    .timezone_id = "Etc/UTC",
    .location_name = "Lab",
    .latitude = 0.0f,
    .longitude = 0.0f,
};

static RuntimeEventSubscription make_subscription(RuntimeServiceEventHandler handler, void *ctx)
{
    RuntimeEventSubscription subscription = {
        .handler = handler,
        .ctx = ctx,
        .name = "host-test",
        .priority = 0,
        .critical = true,
    };
    return subscription;
}

static bool config_event_handler(RuntimeServiceEvent event, void *ctx)
{
    (void)ctx;
    assert(event == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
    return g_event_should_succeed;
}

bool device_config_get(DeviceConfigSnapshot *out)
{
    if (!g_config_ok || out == NULL) {
        return false;
    }
    *out = g_cfg;
    return true;
}

uint32_t device_config_generation(void)
{
    return g_config_generation;
}

uint32_t web_wifi_last_applied_generation(void)
{
    return g_wifi_verify_ok ? g_config_generation : (g_config_generation == 0U ? 0U : g_config_generation - 1U);
}

bool web_wifi_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_wifi_verify_ok && generation == g_config_generation;
}

uint32_t network_sync_service_last_applied_generation(void)
{
    return g_network_verify_ok ? g_config_generation : (g_config_generation == 0U ? 0U : g_config_generation - 1U);
}

bool network_sync_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_network_verify_ok && generation == g_config_generation;
}

uint32_t display_service_last_applied_generation(void)
{
    return g_display_verify_ok ? g_config_generation : (g_config_generation == 0U ? 0U : g_config_generation - 1U);
}

bool display_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_display_verify_ok && generation == g_config_generation;
}

uint32_t power_service_last_applied_generation(void)
{
    return g_power_verify_ok ? g_config_generation : (g_config_generation == 0U ? 0U : g_config_generation - 1U);
}

bool power_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_power_verify_ok && generation == g_config_generation;
}

uint32_t sensor_service_last_applied_generation(void)
{
    return g_sensor_verify_ok ? g_config_generation : (g_config_generation == 0U ? 0U : g_config_generation - 1U);
}

bool sensor_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg)
{
    (void)cfg;
    return g_sensor_verify_ok && generation == g_config_generation;
}

int main(void)
{
    RuntimeReloadReport report = {0};

    assert((runtime_reload_supported_domain_mask() & (RUNTIME_RELOAD_DOMAIN_WIFI |
                                                      RUNTIME_RELOAD_DOMAIN_NETWORK |
                                                      RUNTIME_RELOAD_DOMAIN_AUTH |
                                                      RUNTIME_RELOAD_DOMAIN_DISPLAY |
                                                      RUNTIME_RELOAD_DOMAIN_POWER |
                                                      RUNTIME_RELOAD_DOMAIN_SENSOR |
                                                      RUNTIME_RELOAD_DOMAIN_COMPANION)) ==
           (RUNTIME_RELOAD_DOMAIN_WIFI |
            RUNTIME_RELOAD_DOMAIN_NETWORK |
            RUNTIME_RELOAD_DOMAIN_AUTH |
            RUNTIME_RELOAD_DOMAIN_DISPLAY |
            RUNTIME_RELOAD_DOMAIN_POWER |
            RUNTIME_RELOAD_DOMAIN_SENSOR |
            RUNTIME_RELOAD_DOMAIN_COMPANION));

    runtime_event_service_reset();
    assert(runtime_reload_device_config(false, false, &report));
    assert(!report.requested);
    assert(report.preflight_ok);
    assert(!report.apply_attempted);
    assert(report.verify_ok);
    assert(!report.partial_success);
    assert(report.wifi_reload_ok);
    assert(report.network_reload_ok);
    assert(strcmp(report.failure_phase, "NONE") == 0);
    assert(strcmp(report.failure_code, "NONE") == 0);
    assert(strcmp(report.wifi_verify_reason, "NONE") == 0);
    assert(strcmp(report.network_verify_reason, "NONE") == 0);
    assert(report.requested_domain_mask == 0U);
    assert(report.applied_domain_mask == 0U);
    assert(report.failed_domain_mask == 0U);

    runtime_event_service_reset();
    { RuntimeEventSubscription subscription = make_subscription(config_event_handler, NULL); assert(runtime_event_service_register_ex(&subscription)); }
    g_event_should_succeed = true;
    g_wifi_verify_ok = true;
    g_network_verify_ok = true;
    g_config_ok = true;
    g_config_generation = 11U;
    report = (RuntimeReloadReport){0};
    assert(runtime_reload_device_config(true, true, &report));
    assert(report.requested);
    assert(report.preflight_ok);
    assert(report.apply_attempted);
    assert(report.event_dispatch_ok);
    assert(report.verify_ok);
    assert(!report.partial_success);
    assert(report.handler_count == 1U);
    assert(report.handler_success_count == 1U);
    assert(report.handler_failure_count == 0U);
    assert(report.handler_critical_failure_count == 0U);
    assert(report.first_failed_handler_name != NULL);
    assert(report.config_generation == 11U);
    assert(report.wifi_applied_generation == 11U);
    assert(report.network_applied_generation == 11U);
    assert(strcmp(report.wifi_verify_reason, "CONFIG_APPLIED") == 0);
    assert(strcmp(report.network_verify_reason, "CONFIG_APPLIED") == 0);
    assert(strcmp(report.failure_phase, "NONE") == 0);
    assert(strcmp(report.failure_code, "NONE") == 0);
    assert(report.requested_domain_mask == (RUNTIME_RELOAD_DOMAIN_WIFI | RUNTIME_RELOAD_DOMAIN_NETWORK));
    assert(report.applied_domain_mask == (RUNTIME_RELOAD_DOMAIN_WIFI | RUNTIME_RELOAD_DOMAIN_NETWORK));
    assert(report.failed_domain_mask == 0U);

    runtime_event_service_reset();
    { RuntimeEventSubscription subscription = make_subscription(config_event_handler, NULL); assert(runtime_event_service_register_ex(&subscription)); }
    g_event_should_succeed = false;
    report = (RuntimeReloadReport){0};
    assert(!runtime_reload_device_config(true, true, &report));
    assert(report.requested);
    assert(report.preflight_ok);
    assert(report.apply_attempted);
    assert(!report.event_dispatch_ok);
    assert(!report.verify_ok);
    assert(!report.partial_success);
    assert(!report.wifi_reload_ok);
    assert(!report.network_reload_ok);
    assert(report.handler_success_count == 0U);
    assert(report.handler_failure_count == 1U);
    assert(report.handler_critical_failure_count == 1U);
    assert(report.first_failed_handler_name != NULL);
    assert(strcmp(report.failure_phase, "DISPATCH") == 0);
    assert(strcmp(report.failure_code, "EVENT_DISPATCH_FAILED") == 0);
    assert(report.requested_domain_mask == (RUNTIME_RELOAD_DOMAIN_WIFI | RUNTIME_RELOAD_DOMAIN_NETWORK));
    assert(report.applied_domain_mask == 0U);
    assert(report.failed_domain_mask == (RUNTIME_RELOAD_DOMAIN_WIFI | RUNTIME_RELOAD_DOMAIN_NETWORK));

    runtime_event_service_reset();
    g_event_should_succeed = true;
    report = (RuntimeReloadReport){0};
    assert(!runtime_reload_device_config(true, false, &report));
    assert(report.requested);
    assert(!report.preflight_ok);
    assert(!report.apply_attempted);
    assert(!report.event_dispatch_ok);
    assert(strcmp(report.failure_phase, "PREFLIGHT") == 0);
    assert(strcmp(report.failure_code, "NO_EVENT_SUBSCRIBERS") == 0);
    assert(report.requested_domain_mask == RUNTIME_RELOAD_DOMAIN_WIFI);
    assert(report.applied_domain_mask == 0U);
    assert(report.failed_domain_mask == RUNTIME_RELOAD_DOMAIN_WIFI);

    runtime_event_service_reset();
    { RuntimeEventSubscription subscription = make_subscription(config_event_handler, NULL); assert(runtime_event_service_register_ex(&subscription)); }
    g_event_should_succeed = true;
    g_wifi_verify_ok = false;
    g_network_verify_ok = true;
    report = (RuntimeReloadReport){0};
    assert(!runtime_reload_device_config(true, false, &report));
    assert(report.requested);
    assert(report.preflight_ok);
    assert(report.apply_attempted);
    assert(report.event_dispatch_ok);
    assert(!report.verify_ok);
    assert(!report.partial_success);
    assert(strcmp(report.failure_phase, "VERIFY") == 0);
    assert(strcmp(report.failure_code, "VERIFY_WIFI_CONFIG_MISMATCH") == 0);
    assert(strcmp(report.wifi_verify_reason, "CONFIG_MISMATCH") == 0);
    assert(report.requested_domain_mask == RUNTIME_RELOAD_DOMAIN_WIFI);
    assert(report.applied_domain_mask == 0U);
    assert(report.failed_domain_mask == RUNTIME_RELOAD_DOMAIN_WIFI);

    runtime_event_service_reset();
    { RuntimeEventSubscription subscription = make_subscription(config_event_handler, NULL); assert(runtime_event_service_register_ex(&subscription)); }
    g_wifi_verify_ok = true;
    g_network_verify_ok = true;
    g_config_ok = false;
    g_config_generation = 0U;
    report = (RuntimeReloadReport){0};
    assert(!runtime_reload_device_config(true, true, &report));
    assert(strcmp(report.failure_code, "VERIFY_CONFIG_UNAVAILABLE") == 0);
    assert(strcmp(report.wifi_verify_reason, "CONFIG_UNAVAILABLE") == 0);
    assert(strcmp(report.network_verify_reason, "CONFIG_UNAVAILABLE") == 0);


    runtime_event_service_reset();
    g_config_ok = true;
    g_config_generation = 13U;
    report = (RuntimeReloadReport){0};
    assert(runtime_reload_device_config_domains(RUNTIME_RELOAD_DOMAIN_AUTH, &report));
    assert(report.requested);
    assert(report.preflight_ok);
    assert(report.apply_attempted);
    assert(report.event_dispatch_ok);
    assert(report.verify_ok);
    assert(report.deferred_domain_mask == RUNTIME_RELOAD_DOMAIN_AUTH);
    assert(report.applied_domain_mask == 0U);
    assert(report.reboot_required_domain_mask == 0U);
    assert(report.failed_domain_mask == 0U);
    assert(report.domain_result_count == 1U);
    assert(report.domain_results[0].persisted_only);
    assert(report.domain_results[0].effective);
    assert(strcmp(report.domain_results[0].verify_reason, "PERSISTED_ONLY") == 0);

    runtime_event_service_reset();
    report = (RuntimeReloadReport){0};
    assert(runtime_reload_device_config_domains(RUNTIME_RELOAD_DOMAIN_COMPANION, &report));
    assert(report.requested);
    assert(report.preflight_ok);
    assert(report.apply_attempted);
    assert(report.event_dispatch_ok);
    assert(report.verify_ok);
    assert(report.requires_reboot);
    assert(report.reboot_required_domain_mask == RUNTIME_RELOAD_DOMAIN_COMPANION);
    assert(report.failed_domain_mask == 0U);
    assert(report.domain_results[0].reboot_required);
    assert(strcmp(report.domain_results[0].verify_reason, "REQUIRES_REBOOT") == 0);

    runtime_event_service_reset();
    { RuntimeEventSubscription subscription = make_subscription(config_event_handler, NULL); subscription.domain_mask = RUNTIME_RELOAD_DOMAIN_DISPLAY | RUNTIME_RELOAD_DOMAIN_POWER | RUNTIME_RELOAD_DOMAIN_SENSOR; assert(runtime_event_service_register_ex(&subscription)); }
    g_display_verify_ok = true;
    g_power_verify_ok = true;
    g_sensor_verify_ok = true;
    report = (RuntimeReloadReport){0};
    assert(runtime_reload_device_config_domains(RUNTIME_RELOAD_DOMAIN_DISPLAY | RUNTIME_RELOAD_DOMAIN_POWER | RUNTIME_RELOAD_DOMAIN_SENSOR, &report));
    assert(report.applied_domain_mask == (RUNTIME_RELOAD_DOMAIN_DISPLAY | RUNTIME_RELOAD_DOMAIN_POWER | RUNTIME_RELOAD_DOMAIN_SENSOR));
    assert(report.failed_domain_mask == 0U);

    puts("[OK] runtime_reload_coordinator behavior check passed");
    return 0;
}
