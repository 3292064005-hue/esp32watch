#ifndef SERVICES_RUNTIME_RELOAD_COORDINATOR_H
#define SERVICES_RUNTIME_RELOAD_COORDINATOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RUNTIME_RELOAD_DOMAIN_NONE = 0,
    RUNTIME_RELOAD_DOMAIN_WIFI = 1 << 0,
    RUNTIME_RELOAD_DOMAIN_NETWORK = 1 << 1,
    RUNTIME_RELOAD_DOMAIN_AUTH = 1 << 2,
    RUNTIME_RELOAD_DOMAIN_DISPLAY = 1 << 3,
    RUNTIME_RELOAD_DOMAIN_POWER = 1 << 4,
    RUNTIME_RELOAD_DOMAIN_SENSOR = 1 << 5,
    RUNTIME_RELOAD_DOMAIN_COMPANION = 1 << 6
} RuntimeReloadDomainMask;

typedef enum {
    RUNTIME_RELOAD_APPLY_HOT = 0,
    RUNTIME_RELOAD_APPLY_PERSISTED_ONLY,
    RUNTIME_RELOAD_APPLY_REQUIRES_REBOOT
} RuntimeReloadApplyStrategy;

typedef struct {
    uint32_t domain_mask;
    const char *domain_name;
    const char *apply_strategy;
    bool requested;
    bool supported;
    bool dispatch_matched;
    bool applied;
    bool verify_ok;
    bool persisted_only;
    bool reboot_required;
    bool effective;
    uint32_t applied_generation;
    const char *verify_reason;
} RuntimeReloadDomainResult;

#define RUNTIME_RELOAD_REPORT_DOMAIN_CAPACITY 8U

typedef struct {
    bool requested;
    bool preflight_ok;
    bool apply_attempted;
    bool event_dispatch_ok;
    bool verify_ok;
    bool partial_success;
    bool wifi_reload_ok;
    bool network_reload_ok;
    uint8_t handler_count;
    uint8_t matched_handler_count;
    uint8_t handler_success_count;
    uint8_t handler_failure_count;
    uint8_t handler_critical_failure_count;
    uint32_t requested_domain_mask;
    uint32_t applied_domain_mask;
    uint32_t deferred_domain_mask;
    uint32_t reboot_required_domain_mask;
    uint32_t failed_domain_mask;
    uint32_t config_generation;
    uint32_t wifi_applied_generation;
    uint32_t network_applied_generation;
    bool requires_reboot;
    uint8_t domain_result_count;
    RuntimeReloadDomainResult domain_results[RUNTIME_RELOAD_REPORT_DOMAIN_CAPACITY];
    const char *first_failed_handler_name;
    const char *failure_phase;
    const char *failure_code;
    const char *wifi_verify_reason;
    const char *network_verify_reason;
} RuntimeReloadReport;

const char *runtime_reload_domain_name(uint32_t domain_mask);
const char *runtime_reload_apply_strategy_name(RuntimeReloadApplyStrategy strategy);
uint32_t runtime_reload_supported_domain_mask(void);
bool runtime_reload_device_config_domains(uint32_t requested_domain_mask,
                                          RuntimeReloadReport *out);
bool runtime_reload_device_config(bool wifi_changed,
                                  bool network_changed,
                                  RuntimeReloadReport *out);

#ifdef __cplusplus
}
#endif

#endif
