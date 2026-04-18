#ifndef RUNTIME_EVENT_SERVICE_H
#define RUNTIME_EVENT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RUNTIME_SERVICE_EVENT_NONE = 0,
    RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED,
    RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED,
    RUNTIME_SERVICE_EVENT_STORAGE_COMMIT_FINISHED,
    RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED,
    RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED,
    RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED
} RuntimeServiceEvent;

typedef bool (*RuntimeServiceEventHandler)(RuntimeServiceEvent event, void *ctx);

typedef struct {
    RuntimeServiceEventHandler handler;
    void *ctx;
    const char *name;
    int8_t priority;
    bool critical;
} RuntimeEventSubscription;

typedef struct {
    bool event_valid;
    bool require_handler;
    bool saw_handler;
    bool all_succeeded;
    uint8_t handler_count;
    uint8_t success_count;
    uint8_t failure_count;
    uint8_t critical_failure_count;
    int8_t first_failed_handler_index;
} RuntimeEventDispatchReport;

void runtime_event_service_reset(void);

bool runtime_event_service_register_ex(const RuntimeEventSubscription *subscription);
bool runtime_event_service_unregister(RuntimeServiceEventHandler handler, void *ctx);
bool runtime_event_service_publish(RuntimeServiceEvent event);
bool runtime_event_service_publish_notify(RuntimeServiceEvent event);
uint8_t runtime_event_service_handler_count(void);
uint8_t runtime_event_service_capacity(void);
uint32_t runtime_event_service_registration_reject_count(void);
uint8_t runtime_event_service_last_success_count(void);
uint8_t runtime_event_service_last_failure_count(void);
uint8_t runtime_event_service_last_critical_failure_count(void);
const char *runtime_event_service_event_name(RuntimeServiceEvent event);
uint32_t runtime_event_service_publish_count(void);
uint32_t runtime_event_service_publish_fail_count(void);
RuntimeServiceEvent runtime_event_service_last_event(void);
RuntimeServiceEvent runtime_event_service_last_failed_event(void);
int8_t runtime_event_service_last_failed_handler_index(void);
bool runtime_event_service_get_last_dispatch(RuntimeEventDispatchReport *out);
const char *runtime_event_service_handler_name(uint8_t index);
int8_t runtime_event_service_handler_priority(uint8_t index);
bool runtime_event_service_handler_is_critical(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif
