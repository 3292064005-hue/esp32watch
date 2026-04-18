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
    RUNTIME_RELOAD_DOMAIN_NETWORK = 1 << 1
} RuntimeReloadDomainMask;

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
    uint8_t handler_success_count;
    uint8_t handler_failure_count;
    uint8_t handler_critical_failure_count;
    uint32_t requested_domain_mask;
    uint32_t applied_domain_mask;
    uint32_t failed_domain_mask;
    uint32_t config_generation;
    uint32_t wifi_applied_generation;
    uint32_t network_applied_generation;
    const char *first_failed_handler_name;
    const char *failure_phase;
    const char *failure_code;
    const char *wifi_verify_reason;
    const char *network_verify_reason;
} RuntimeReloadReport;

/**
 * @brief Reload runtime services that depend on the persisted device configuration.
 *
 * The coordinator runs a preflight check, publishes the canonical config-change
 * event through the authoritative runtime-event subscriber path, and records a
 * verify phase outcome for telemetry and API reporting.
 *
 * @param[in] wifi_changed true when Wi-Fi credentials or provisioning mode changed.
 * @param[in] network_changed true when time/weather network profile changed.
 * @param[out] out Optional detailed report describing dispatch and verify behavior.
 * @return true when every requested runtime service reloaded successfully.
 * @throws None.
 * @boundary_behavior Returns true without side effects when neither @p wifi_changed nor @p network_changed is set.
 */
bool runtime_reload_device_config(bool wifi_changed,
                                  bool network_changed,
                                  RuntimeReloadReport *out);

#ifdef __cplusplus
}
#endif

#endif
