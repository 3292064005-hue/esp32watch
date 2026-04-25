#include "services/runtime_reload_event_manifest.h"

static const RuntimeReloadConsumerManifestEntry kReloadConsumers[] = {
    {RUNTIME_RELOAD_DOMAIN_WIFI, "WIFI", RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
     "web_wifi", "src/web/web_wifi.cpp", "HOT_APPLY",
     "web_wifi_verify_config_applied", "web_wifi_last_applied_generation", true, true},
    {RUNTIME_RELOAD_DOMAIN_NETWORK, "NETWORK", RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
     "network_sync_service", "src/services/network_sync_service.cpp", "HOT_APPLY",
     "network_sync_service_verify_config_applied", "network_sync_service_last_applied_generation", true, true},
    {RUNTIME_RELOAD_DOMAIN_AUTH, "AUTH", RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
     "device_config_authority", "src/services/device_config_authority.c", "PERSISTED_ONLY",
     "runtime_reload_passthrough_verify", "runtime_reload_passthrough_generation", false, false},
    {RUNTIME_RELOAD_DOMAIN_DISPLAY, "DISPLAY", RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
     "display_service", "src/services/display_service.c", "HOT_APPLY",
     "display_service_verify_config_applied", "display_service_last_applied_generation", true, true},
    {RUNTIME_RELOAD_DOMAIN_POWER, "POWER", RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
     "power_service", "src/services/power_service.c", "HOT_APPLY",
     "power_service_verify_config_applied", "power_service_last_applied_generation", true, true},
    {RUNTIME_RELOAD_DOMAIN_SENSOR, "SENSOR", RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
     "sensor_service", "src/services/sensor_service.c", "HOT_APPLY",
     "sensor_service_verify_config_applied", "sensor_service_last_applied_generation", true, true},
    {RUNTIME_RELOAD_DOMAIN_COMPANION, "COMPANION", RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
     "companion_runtime", "src/companion_transport.c", "REQUIRES_REBOOT",
     "runtime_reload_passthrough_verify", "runtime_reload_passthrough_generation", false, false},
};

static const RuntimeEventProducerManifestEntry kEventProducers[] = {
    {RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED, "RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED",
     "device_config_authority", "src/services/device_config_authority.c", "hot-apply reload consumers", true},
    {RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED, "RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED",
     "power_service", "src/services/power_service.c", "diagnostics / observers", false},
    {RUNTIME_SERVICE_EVENT_STORAGE_COMMIT_FINISHED, "RUNTIME_SERVICE_EVENT_STORAGE_COMMIT_FINISHED",
     "storage_service", "src/services/storage_service.c", "diagnostics / observers", false},
    {RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED, "RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED",
     "reset_service", "src/services/reset_service.c", "audit / diagnostics", false},
    {RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED, "RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED",
     "reset_service", "src/services/reset_service.c", "audit / diagnostics", false},
    {RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED, "RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED",
     "reset_service", "src/services/reset_service.c", "audit / diagnostics", false},
};

const RuntimeReloadConsumerManifestEntry *runtime_reload_consumer_manifest(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(kReloadConsumers) / sizeof(kReloadConsumers[0]);
    }
    return kReloadConsumers;
}

const RuntimeEventProducerManifestEntry *runtime_event_producer_manifest(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(kEventProducers) / sizeof(kEventProducers[0]);
    }
    return kEventProducers;
}

const RuntimeReloadConsumerManifestEntry *runtime_reload_consumer_manifest_find(uint32_t domain_mask)
{
    size_t count = 0U;
    const RuntimeReloadConsumerManifestEntry *entries = runtime_reload_consumer_manifest(&count);
    size_t i;
    for (i = 0U; i < count; ++i) {
        if (entries[i].domain_mask == domain_mask) {
            return &entries[i];
        }
    }
    return NULL;
}

const RuntimeEventProducerManifestEntry *runtime_event_producer_manifest_find(RuntimeServiceEvent event)
{
    size_t count = 0U;
    const RuntimeEventProducerManifestEntry *entries = runtime_event_producer_manifest(&count);
    size_t i;
    for (i = 0U; i < count; ++i) {
        if (entries[i].event == event) {
            return &entries[i];
        }
    }
    return NULL;
}
