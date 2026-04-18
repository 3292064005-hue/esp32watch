#include "services/runtime_event_service.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int g_order = 0;
static int g_first_seen = 0;
static int g_second_seen = 0;

static bool first_handler(RuntimeServiceEvent event, void *ctx)
{
    (void)ctx;
    assert(event == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
    g_first_seen = ++g_order;
    return true;
}

static bool second_handler(RuntimeServiceEvent event, void *ctx)
{
    (void)ctx;
    assert(event == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
    g_second_seen = ++g_order;
    return true;
}

static bool noop_handler(RuntimeServiceEvent event, void *ctx)
{
    (void)event;
    (void)ctx;
    return false;
}

static bool mixed_success_handler(RuntimeServiceEvent event, void *ctx)
{
    uintptr_t idx = (uintptr_t)ctx;
    assert(event == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED ||
           event == RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED);
    return idx == 0U;
}

static RuntimeEventSubscription make_subscription(RuntimeServiceEventHandler handler,
                                                  void *ctx,
                                                  const char *name,
                                                  int8_t priority,
                                                  bool critical)
{
    RuntimeEventSubscription subscription = {
        .handler = handler,
        .ctx = ctx,
        .name = name,
        .priority = priority,
        .critical = critical,
    };
    return subscription;
}

int main(void)
{
    RuntimeEventSubscription first = make_subscription(first_handler, NULL, "first", 2, true);
    RuntimeEventSubscription second = make_subscription(second_handler, NULL, "second", 1, true);

    runtime_event_service_reset();
    assert(runtime_event_service_handler_count() == 0U);
    assert(runtime_event_service_publish_count() == 0U);
    assert(runtime_event_service_publish_fail_count() == 0U);
    assert(runtime_event_service_last_event() == RUNTIME_SERVICE_EVENT_NONE);
    assert(runtime_event_service_last_failed_event() == RUNTIME_SERVICE_EVENT_NONE);
    assert(runtime_event_service_last_failed_handler_index() == -1);
    assert(!runtime_event_service_publish(RUNTIME_SERVICE_EVENT_NONE));

    assert(runtime_event_service_register_ex(&first));
    assert(runtime_event_service_register_ex(&second));
    assert(runtime_event_service_register_ex(&first));
    assert(runtime_event_service_handler_count() == 2U);
    assert(runtime_event_service_unregister(second_handler, NULL));
    assert(runtime_event_service_handler_count() == 1U);
    assert(runtime_event_service_register_ex(&second));
    assert(runtime_event_service_handler_count() == 2U);
    assert(runtime_event_service_publish(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED));
    assert(runtime_event_service_publish_count() == 1U);
    assert(runtime_event_service_publish_fail_count() == 0U);
    assert(runtime_event_service_last_event() == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
    assert(runtime_event_service_last_failed_event() == RUNTIME_SERVICE_EVENT_NONE);
    assert(g_first_seen == 1);
    assert(g_second_seen == 2);

    runtime_event_service_reset();
    {
        RuntimeEventSubscription ok = make_subscription(mixed_success_handler, (void *)(uintptr_t)0U, "ok", 1, true);
        RuntimeEventSubscription fail = make_subscription(mixed_success_handler, (void *)(uintptr_t)1U, "fail", 0, true);
        assert(runtime_event_service_register_ex(&ok));
        assert(runtime_event_service_register_ex(&fail));
    }
    assert(!runtime_event_service_publish(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED));
    assert(runtime_event_service_publish_count() == 1U);
    assert(runtime_event_service_publish_fail_count() == 1U);
    assert(runtime_event_service_last_failed_event() == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);
    assert(runtime_event_service_last_failed_handler_index() == 1);

    runtime_event_service_reset();
    for (unsigned i = 0; i < runtime_event_service_capacity(); ++i) {
        RuntimeEventSubscription subscription = make_subscription(noop_handler, (void *)(uintptr_t)i, "noop", 0, true);
        assert(runtime_event_service_register_ex(&subscription));
    }
    assert(runtime_event_service_handler_count() == runtime_event_service_capacity());
    {
        RuntimeEventSubscription overflow = make_subscription(noop_handler, (void *)(uintptr_t)99U, "overflow", 0, true);
        assert(!runtime_event_service_register_ex(&overflow));
    }
    assert(runtime_event_service_registration_reject_count() == 1U);
    assert(!runtime_event_service_publish(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED));
    assert(runtime_event_service_publish_count() == 1U);
    assert(runtime_event_service_publish_fail_count() == 1U);
    assert(runtime_event_service_last_failed_event() == RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED);

    runtime_event_service_reset();
    assert(runtime_event_service_publish_notify(RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED));
    assert(runtime_event_service_publish_count() == 1U);
    assert(runtime_event_service_publish_fail_count() == 0U);
    assert(runtime_event_service_last_success_count() == 0U);
    assert(runtime_event_service_last_failure_count() == 0U);
    assert(runtime_event_service_last_event() == RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED);

    runtime_event_service_reset();
    {
        RuntimeEventSubscription ok = make_subscription(mixed_success_handler, (void *)(uintptr_t)0U, "ok", 1, true);
        RuntimeEventSubscription fail = make_subscription(mixed_success_handler, (void *)(uintptr_t)1U, "fail", 0, true);
        assert(runtime_event_service_register_ex(&ok));
        assert(runtime_event_service_register_ex(&fail));
    }
    assert(!runtime_event_service_publish_notify(RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED));
    assert(runtime_event_service_publish_count() == 1U);
    assert(runtime_event_service_publish_fail_count() == 1U);
    assert(runtime_event_service_last_success_count() == 1U);
    assert(runtime_event_service_last_failure_count() == 1U);
    assert(runtime_event_service_last_failed_event() == RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED);
    assert(runtime_event_service_last_critical_failure_count() == 2U || runtime_event_service_last_critical_failure_count() == 1U);

    puts("[OK] runtime_event_service behavior check passed");
    return 0;
}
