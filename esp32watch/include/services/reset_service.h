#ifndef SERVICES_RESET_SERVICE_H
#define SERVICES_RESET_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "services/runtime_reload_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RESET_ACTION_APP_STATE = 0,
    RESET_ACTION_DEVICE_CONFIG,
    RESET_ACTION_FACTORY
} ResetActionKind;

typedef struct {
    ResetActionKind kind;
    bool app_state_reset;
    bool device_config_reset;
    bool runtime_reload_requested;
    bool runtime_reload_ok;
    bool audit_notified;
    bool audit_event_dispatched;
    RuntimeReloadReport reload;
} ResetActionReport;

/**
 * @brief Reset only watch application state owned by the runtime model and storage service.
 *
 * @param[out] out Optional execution report.
 * @return true when the reset command was applied.
 * @throws None.
 * @boundary_behavior The report marks audit_event_dispatched when at least one best-effort runtime audit handler accepted the completion event.
 */
bool reset_service_reset_app_state(ResetActionReport *out);

/**
 * @brief Reset only persisted device configuration such as Wi-Fi, timezone, location, and API token.
 *
 * @param[out] out Optional execution report.
 * @return true when the persisted configuration was restored and dependent runtime services reloaded.
 * @throws None.
 * @boundary_behavior The report marks audit_event_dispatched when the completion event reached at least one best-effort runtime audit handler.
 */
bool reset_service_reset_device_config(ResetActionReport *out);

/**
 * @brief Perform a full factory reset that combines app-state reset and device-config reset.
 *
 * @param[out] out Optional execution report.
 * @return true when both reset domains completed successfully.
 * @throws None.
 * @boundary_behavior The report marks audit_event_dispatched when the completion event reached at least one best-effort runtime audit handler.
 */
bool reset_service_factory_reset(ResetActionReport *out);

#ifdef __cplusplus
}
#endif

#endif
