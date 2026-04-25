#include "services/runtime_reload_event_manifest.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static bool has_domain(uint32_t mask)
{
    return runtime_reload_consumer_manifest_find(mask) != NULL;
}

static bool has_event(RuntimeServiceEvent event)
{
    return runtime_event_producer_manifest_find(event) != NULL;
}

int main(void)
{
    size_t reload_count = 0U;
    const RuntimeReloadConsumerManifestEntry *reloads = runtime_reload_consumer_manifest(&reload_count);
    assert(reloads != NULL);
    assert(reload_count == 7U);
    assert(has_domain(RUNTIME_RELOAD_DOMAIN_WIFI));
    assert(has_domain(RUNTIME_RELOAD_DOMAIN_NETWORK));
    assert(has_domain(RUNTIME_RELOAD_DOMAIN_AUTH));
    assert(has_domain(RUNTIME_RELOAD_DOMAIN_DISPLAY));
    assert(has_domain(RUNTIME_RELOAD_DOMAIN_POWER));
    assert(has_domain(RUNTIME_RELOAD_DOMAIN_SENSOR));
    assert(has_domain(RUNTIME_RELOAD_DOMAIN_COMPANION));
    assert(runtime_reload_consumer_manifest_find(RUNTIME_RELOAD_DOMAIN_NONE) == NULL);

    for (size_t i = 0U; i < reload_count; ++i) {
        assert(reloads[i].domain_mask != RUNTIME_RELOAD_DOMAIN_NONE);
        assert(reloads[i].domain_name != NULL && reloads[i].domain_name[0] != '\0');
        assert(reloads[i].consumer_name != NULL && reloads[i].consumer_name[0] != '\0');
        assert(reloads[i].source_file != NULL && reloads[i].source_file[0] != '\0');
        assert(reloads[i].apply_strategy != NULL && reloads[i].apply_strategy[0] != '\0');
        assert(reloads[i].verify_symbol != NULL && reloads[i].verify_symbol[0] != '\0');
        assert(reloads[i].applied_generation_symbol != NULL && reloads[i].applied_generation_symbol[0] != '\0');
        if (strcmp(reloads[i].apply_strategy, "HOT_APPLY") == 0) {
            assert(reloads[i].requires_subscription);
            assert(reloads[i].strong_business_consumer);
            assert(reloads[i].subscription_event == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
        } else {
            assert(!reloads[i].requires_subscription);
            assert(!reloads[i].strong_business_consumer);
        }
    }

    size_t event_count = 0U;
    const RuntimeEventProducerManifestEntry *events = runtime_event_producer_manifest(&event_count);
    assert(events != NULL);
    assert(event_count == 6U);
    assert(has_event(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED));
    assert(has_event(RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED));
    assert(has_event(RUNTIME_SERVICE_EVENT_STORAGE_COMMIT_FINISHED));
    assert(has_event(RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED));
    assert(has_event(RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED));
    assert(has_event(RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED));
    assert(runtime_event_producer_manifest_find(RUNTIME_SERVICE_EVENT_NONE) == NULL);

    const RuntimeEventProducerManifestEntry *device_config =
        runtime_event_producer_manifest_find(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
    assert(device_config != NULL);
    assert(device_config->strong_business_consumer_required);
    assert(strcmp(device_config->consumer_class, "hot-apply reload consumers") == 0);

    return 0;
}
