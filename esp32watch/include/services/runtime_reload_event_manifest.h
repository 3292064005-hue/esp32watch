#ifndef SERVICES_RUNTIME_RELOAD_EVENT_MANIFEST_H
#define SERVICES_RUNTIME_RELOAD_EVENT_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "services/runtime_event_service.h"
#include "services/runtime_reload_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t domain_mask;
    const char *domain_name;
    RuntimeServiceEvent subscription_event;
    const char *consumer_name;
    const char *source_file;
    const char *apply_strategy;
    const char *verify_symbol;
    const char *applied_generation_symbol;
    bool requires_subscription;
    bool strong_business_consumer;
} RuntimeReloadConsumerManifestEntry;

typedef struct {
    RuntimeServiceEvent event;
    const char *event_name;
    const char *producer_name;
    const char *producer_file;
    const char *consumer_class;
    bool strong_business_consumer_required;
} RuntimeEventProducerManifestEntry;

/**
 * Return the canonical runtime reload consumer manifest.
 *
 * The manifest is a static, read-only description used by documentation, host
 * validation, and diagnostic control-plane surfaces. The returned entries remain
 * valid for the process lifetime. Passing NULL is allowed when only the pointer
 * is needed; otherwise *count receives the number of entries.
 */
const RuntimeReloadConsumerManifestEntry *runtime_reload_consumer_manifest(size_t *count);

/**
 * Return the canonical runtime event producer/consumer-class manifest.
 *
 * Entries describe which component publishes each runtime service event and
 * whether a strong business consumer is required. Events marked as audit-only
 * deliberately do not require a hot-apply subscriber. The returned entries are
 * static and remain valid for the process lifetime.
 */
const RuntimeEventProducerManifestEntry *runtime_event_producer_manifest(size_t *count);

/**
 * Lookup a reload consumer manifest row by exact domain mask.
 *
 * Returns NULL when the domain is unknown or RUNTIME_RELOAD_DOMAIN_NONE. This is
 * a pure lookup and never mutates runtime state.
 */
const RuntimeReloadConsumerManifestEntry *runtime_reload_consumer_manifest_find(uint32_t domain_mask);

/**
 * Lookup an event producer manifest row by event id.
 *
 * Returns NULL for RUNTIME_SERVICE_EVENT_NONE or unknown values. This is a pure
 * lookup and never publishes events.
 */
const RuntimeEventProducerManifestEntry *runtime_event_producer_manifest_find(RuntimeServiceEvent event);

#ifdef __cplusplus
}
#endif

#endif
