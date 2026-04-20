#include "services/runtime_event_service.h"
#include "app_limits.h"
#include "app_config.h"
#include <string.h>

#ifndef RUNTIME_EVENT_HANDLER_MAX
#define RUNTIME_EVENT_HANDLER_MAX APP_RUNTIME_EVENT_HANDLER_CAPACITY
#endif

typedef struct {
    RuntimeServiceEventHandler handler;
    void *ctx;
    const char *name;
    int8_t priority;
    bool critical;
    uint32_t event_mask;
    uint32_t domain_mask;
} RuntimeEventHandlerEntry;

static RuntimeEventHandlerEntry g_runtime_event_handlers[RUNTIME_EVENT_HANDLER_MAX];
static uint32_t g_runtime_event_publish_count;
static uint32_t g_runtime_event_publish_fail_count;
static uint32_t g_runtime_event_registration_reject_count;
static RuntimeServiceEvent g_runtime_event_last_event = RUNTIME_SERVICE_EVENT_NONE;
static RuntimeServiceEvent g_runtime_event_last_failed_event = RUNTIME_SERVICE_EVENT_NONE;
static int8_t g_runtime_event_last_failed_handler_index = -1;
static uint8_t g_runtime_event_last_success_count = 0U;
static uint8_t g_runtime_event_last_failure_count = 0U;
static uint8_t g_runtime_event_last_critical_failure_count = 0U;
static RuntimeEventDispatchReport g_runtime_event_last_dispatch;

uint32_t runtime_event_service_event_mask(RuntimeServiceEvent event)
{
    if (event <= RUNTIME_SERVICE_EVENT_NONE || event >= 32) {
        return 0U;
    }
    return (1UL << (uint32_t)event);
}

static bool runtime_event_service_matches(const RuntimeEventHandlerEntry *entry,
                                         RuntimeServiceEvent event,
                                         uint32_t domain_mask)
{
    if (entry == NULL || entry->handler == NULL) {
        return false;
    }
    if ((entry->event_mask & runtime_event_service_event_mask(event)) == 0U) {
        return false;
    }
    if (domain_mask == RUNTIME_EVENT_DOMAIN_NONE || domain_mask == RUNTIME_EVENT_DOMAIN_ALL) {
        return true;
    }
    if (entry->domain_mask == RUNTIME_EVENT_DOMAIN_NONE || entry->domain_mask == RUNTIME_EVENT_DOMAIN_ALL) {
        return true;
    }
    return (entry->domain_mask & domain_mask) != 0U;
}

static void runtime_event_service_insert_entry(uint8_t index, const RuntimeEventSubscription *subscription)
{
    for (uint8_t move = RUNTIME_EVENT_HANDLER_MAX - 1U; move > index; --move) {
        g_runtime_event_handlers[move] = g_runtime_event_handlers[move - 1U];
    }
    g_runtime_event_handlers[index].handler = subscription->handler;
    g_runtime_event_handlers[index].ctx = subscription->ctx;
    g_runtime_event_handlers[index].name = subscription->name != NULL ? subscription->name : "anonymous";
    g_runtime_event_handlers[index].priority = subscription->priority;
    g_runtime_event_handlers[index].critical = subscription->critical;
    g_runtime_event_handlers[index].event_mask = subscription->event_mask != 0U ? subscription->event_mask : RUNTIME_EVENT_MASK_ALL;
    g_runtime_event_handlers[index].domain_mask = subscription->domain_mask != 0U ? subscription->domain_mask : RUNTIME_EVENT_DOMAIN_ALL;
}

void runtime_event_service_reset(void)
{
    memset(g_runtime_event_handlers, 0, sizeof(g_runtime_event_handlers));
    memset(&g_runtime_event_last_dispatch, 0, sizeof(g_runtime_event_last_dispatch));
    g_runtime_event_publish_count = 0U;
    g_runtime_event_publish_fail_count = 0U;
    g_runtime_event_registration_reject_count = 0U;
    g_runtime_event_last_event = RUNTIME_SERVICE_EVENT_NONE;
    g_runtime_event_last_failed_event = RUNTIME_SERVICE_EVENT_NONE;
    g_runtime_event_last_failed_handler_index = -1;
    g_runtime_event_last_success_count = 0U;
    g_runtime_event_last_failure_count = 0U;
    g_runtime_event_last_critical_failure_count = 0U;
}

bool runtime_event_service_register_ex(const RuntimeEventSubscription *subscription)
{
    RuntimeEventSubscription normalized = {0};
    uint8_t count = 0U;

    if (subscription == NULL || subscription->handler == NULL) {
        ++g_runtime_event_registration_reject_count;
        return false;
    }
    normalized = *subscription;
    if (normalized.name == NULL) {
        normalized.name = "anonymous";
    }
    if (normalized.event_mask == 0U) {
        normalized.event_mask = RUNTIME_EVENT_MASK_ALL;
    }
    if (normalized.domain_mask == 0U) {
        normalized.domain_mask = RUNTIME_EVENT_DOMAIN_ALL;
    }

    for (uint8_t i = 0U; i < RUNTIME_EVENT_HANDLER_MAX; ++i) {
        if (g_runtime_event_handlers[i].handler == normalized.handler &&
            g_runtime_event_handlers[i].ctx == normalized.ctx) {
            g_runtime_event_handlers[i].name = normalized.name;
            g_runtime_event_handlers[i].priority = normalized.priority;
            g_runtime_event_handlers[i].critical = normalized.critical;
            g_runtime_event_handlers[i].event_mask = normalized.event_mask;
            g_runtime_event_handlers[i].domain_mask = normalized.domain_mask;
            return true;
        }
        if (g_runtime_event_handlers[i].handler != NULL) {
            ++count;
        }
    }
    if (count >= RUNTIME_EVENT_HANDLER_MAX) {
        ++g_runtime_event_registration_reject_count;
        return false;
    }

    uint8_t insert_index = count;
    for (uint8_t i = 0U; i < count; ++i) {
        if (normalized.priority > g_runtime_event_handlers[i].priority) {
            insert_index = i;
            break;
        }
    }
    runtime_event_service_insert_entry(insert_index, &normalized);
    return true;
}

bool runtime_event_service_unregister(RuntimeServiceEventHandler handler, void *ctx)
{
    if (handler == NULL) {
        return false;
    }
    for (uint8_t i = 0U; i < RUNTIME_EVENT_HANDLER_MAX; ++i) {
        if (g_runtime_event_handlers[i].handler == handler && g_runtime_event_handlers[i].ctx == ctx) {
            for (uint8_t move = i; move + 1U < RUNTIME_EVENT_HANDLER_MAX; ++move) {
                g_runtime_event_handlers[move] = g_runtime_event_handlers[move + 1U];
            }
            memset(&g_runtime_event_handlers[RUNTIME_EVENT_HANDLER_MAX - 1U], 0, sizeof(g_runtime_event_handlers[0]));
            return true;
        }
    }
    return false;
}

static bool runtime_event_service_dispatch(RuntimeServiceEvent event,
                                           bool require_handler,
                                           uint32_t domain_mask)
{
    bool all_succeeded = true;

    memset(&g_runtime_event_last_dispatch, 0, sizeof(g_runtime_event_last_dispatch));
    g_runtime_event_last_dispatch.event_valid = event > RUNTIME_SERVICE_EVENT_NONE;
    g_runtime_event_last_dispatch.require_handler = require_handler;
    g_runtime_event_last_dispatch.domain_mask = domain_mask;
    g_runtime_event_last_failed_handler_index = -1;
    g_runtime_event_last_success_count = 0U;
    g_runtime_event_last_failure_count = 0U;
    g_runtime_event_last_critical_failure_count = 0U;

    if (event <= RUNTIME_SERVICE_EVENT_NONE) {
        return false;
    }

    ++g_runtime_event_publish_count;
    g_runtime_event_last_event = event;

    for (uint8_t i = 0U; i < RUNTIME_EVENT_HANDLER_MAX; ++i) {
        if (g_runtime_event_handlers[i].handler == NULL) {
            continue;
        }
        if (g_runtime_event_last_dispatch.handler_count < 0xFFU) {
            ++g_runtime_event_last_dispatch.handler_count;
        }
        if (!runtime_event_service_matches(&g_runtime_event_handlers[i], event, domain_mask)) {
            continue;
        }
        g_runtime_event_last_dispatch.saw_handler = true;
        if (g_runtime_event_last_dispatch.matched_handler_count < 0xFFU) {
            ++g_runtime_event_last_dispatch.matched_handler_count;
        }
        if (g_runtime_event_handlers[i].handler(event, g_runtime_event_handlers[i].ctx)) {
            if (g_runtime_event_last_success_count < 0xFFU) {
                ++g_runtime_event_last_success_count;
            }
            continue;
        }

        all_succeeded = false;
        if (g_runtime_event_last_failure_count < 0xFFU) {
            ++g_runtime_event_last_failure_count;
        }
        if (g_runtime_event_last_failed_handler_index < 0) {
            g_runtime_event_last_failed_handler_index = (int8_t)i;
        }
        g_runtime_event_last_failed_event = event;
        if (g_runtime_event_handlers[i].critical && g_runtime_event_last_critical_failure_count < 0xFFU) {
            ++g_runtime_event_last_critical_failure_count;
        }
    }

    g_runtime_event_last_dispatch.success_count = g_runtime_event_last_success_count;
    g_runtime_event_last_dispatch.failure_count = g_runtime_event_last_failure_count;
    g_runtime_event_last_dispatch.critical_failure_count = g_runtime_event_last_critical_failure_count;
    g_runtime_event_last_dispatch.first_failed_handler_index = g_runtime_event_last_failed_handler_index;
    g_runtime_event_last_dispatch.all_succeeded = all_succeeded;

    if ((require_handler && !g_runtime_event_last_dispatch.saw_handler) || !all_succeeded) {
        ++g_runtime_event_publish_fail_count;
    }

    return (!require_handler || g_runtime_event_last_dispatch.saw_handler) && all_succeeded;
}

bool runtime_event_service_publish(RuntimeServiceEvent event)
{
    return runtime_event_service_dispatch(event, true, RUNTIME_EVENT_DOMAIN_ALL);
}

bool runtime_event_service_publish_notify(RuntimeServiceEvent event)
{
    return runtime_event_service_dispatch(event, false, RUNTIME_EVENT_DOMAIN_ALL);
}

bool runtime_event_service_publish_domains(RuntimeServiceEvent event, uint32_t domain_mask)
{
    return runtime_event_service_dispatch(event, true, domain_mask);
}

bool runtime_event_service_publish_notify_domains(RuntimeServiceEvent event, uint32_t domain_mask)
{
    return runtime_event_service_dispatch(event, false, domain_mask);
}

uint8_t runtime_event_service_matching_handler_count(RuntimeServiceEvent event, uint32_t domain_mask)
{
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < RUNTIME_EVENT_HANDLER_MAX; ++i) {
        if (!runtime_event_service_matches(&g_runtime_event_handlers[i], event, domain_mask)) {
            continue;
        }
        if (count < 0xFFU) {
            ++count;
        }
    }
    return count;
}

uint8_t runtime_event_service_handler_count(void)
{
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < RUNTIME_EVENT_HANDLER_MAX; ++i) {
        if (g_runtime_event_handlers[i].handler != NULL) {
            ++count;
        }
    }
    return count;
}

uint8_t runtime_event_service_capacity(void)
{
    return RUNTIME_EVENT_HANDLER_MAX;
}

uint32_t runtime_event_service_registration_reject_count(void)
{
    return g_runtime_event_registration_reject_count;
}

uint32_t runtime_event_service_publish_count(void)
{
    return g_runtime_event_publish_count;
}

uint32_t runtime_event_service_publish_fail_count(void)
{
    return g_runtime_event_publish_fail_count;
}

RuntimeServiceEvent runtime_event_service_last_event(void)
{
    return g_runtime_event_last_event;
}

RuntimeServiceEvent runtime_event_service_last_failed_event(void)
{
    return g_runtime_event_last_failed_event;
}

int8_t runtime_event_service_last_failed_handler_index(void)
{
    return g_runtime_event_last_failed_handler_index;
}

uint8_t runtime_event_service_last_success_count(void)
{
    return g_runtime_event_last_success_count;
}

uint8_t runtime_event_service_last_failure_count(void)
{
    return g_runtime_event_last_failure_count;
}

uint8_t runtime_event_service_last_critical_failure_count(void)
{
    return g_runtime_event_last_critical_failure_count;
}

bool runtime_event_service_get_last_dispatch(RuntimeEventDispatchReport *out)
{
    if (out == NULL) {
        return false;
    }
    *out = g_runtime_event_last_dispatch;
    return true;
}

const char *runtime_event_service_handler_name(uint8_t index)
{
    if (index >= RUNTIME_EVENT_HANDLER_MAX || g_runtime_event_handlers[index].handler == NULL) {
        return "NONE";
    }
    return g_runtime_event_handlers[index].name != NULL ? g_runtime_event_handlers[index].name : "anonymous";
}

int8_t runtime_event_service_handler_priority(uint8_t index)
{
    if (index >= RUNTIME_EVENT_HANDLER_MAX || g_runtime_event_handlers[index].handler == NULL) {
        return -128;
    }
    return g_runtime_event_handlers[index].priority;
}

bool runtime_event_service_handler_is_critical(uint8_t index)
{
    if (index >= RUNTIME_EVENT_HANDLER_MAX || g_runtime_event_handlers[index].handler == NULL) {
        return false;
    }
    return g_runtime_event_handlers[index].critical;
}

uint32_t runtime_event_service_handler_event_mask(uint8_t index)
{
    if (index >= RUNTIME_EVENT_HANDLER_MAX || g_runtime_event_handlers[index].handler == NULL) {
        return 0U;
    }
    return g_runtime_event_handlers[index].event_mask;
}

uint32_t runtime_event_service_handler_domain_mask(uint8_t index)
{
    if (index >= RUNTIME_EVENT_HANDLER_MAX || g_runtime_event_handlers[index].handler == NULL) {
        return 0U;
    }
    return g_runtime_event_handlers[index].domain_mask;
}

const char *runtime_event_service_event_name(RuntimeServiceEvent event)
{
    switch (event) {
        case RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED: return "DEVICE_CONFIG_CHANGED";
        case RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED: return "POWER_STATE_CHANGED";
        case RUNTIME_SERVICE_EVENT_STORAGE_COMMIT_FINISHED: return "STORAGE_COMMIT_FINISHED";
        case RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED: return "RESET_APP_STATE_COMPLETED";
        case RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED: return "RESET_DEVICE_CONFIG_COMPLETED";
        case RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED: return "FACTORY_RESET_COMPLETED";
        default: return "NONE";
    }
}
