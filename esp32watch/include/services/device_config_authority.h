#ifndef SERVICES_DEVICE_CONFIG_AUTHORITY_H
#define SERVICES_DEVICE_CONFIG_AUTHORITY_H

#include <stdbool.h>
#include <stdint.h>
#include "services/device_config.h"
#include "services/runtime_reload_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool committed;
    bool wifi_saved;
    bool network_saved;
    bool token_saved;
    bool wifi_changed;
    bool network_changed;
    bool token_changed;
    bool runtime_reload_requested;
    bool runtime_reload_ok;
    RuntimeReloadReport reload;
} DeviceConfigAuthorityApplyReport;

/**
 * @brief Read the authoritative durable device configuration snapshot.
 *
 * @param[out] out Destination snapshot.
 * @return true when @p out was populated.
 * @throws None.
 */
bool device_config_authority_get(DeviceConfigSnapshot *out);

/**
 * @brief Report whether a mutation token is configured in the authoritative device configuration.
 *
 * @return true when the control plane requires a token for mutating operations.
 * @throws None.
 */
bool device_config_authority_has_token(void);

/**
 * @brief Authenticate a presented mutation token against the authoritative device configuration.
 *
 * @param[in] token Candidate token.
 * @return true when writes are permitted.
 * @throws None.
 */
bool device_config_authority_authenticate_token(const char *token);

/**
 * @brief Validate, persist, diff, and apply a device-configuration update through the single authority path.
 *
 * @param[in] update Requested update payload.
 * @param[out] out Optional apply report describing persistence and runtime-reload results.
 * @return true when persistence succeeded and any required runtime reload also succeeded.
 * @throws None.
 * @boundary_behavior Persists the new snapshot before runtime reload. When persistence succeeds but reload verification fails, the function returns false and reports the degraded apply outcome in @p out.
 */
bool device_config_authority_apply_update(const DeviceConfigUpdate *update,
                                          DeviceConfigAuthorityApplyReport *out);

/**
 * @brief Restore the device configuration defaults and apply dependent runtime reload through the authority path.
 *
 * @param[out] out Optional apply report describing persistence and runtime-reload results.
 * @return true when defaults were persisted and dependent runtime services were reloaded successfully.
 * @throws None.
 */
bool device_config_authority_restore_defaults(DeviceConfigAuthorityApplyReport *out);

#ifdef __cplusplus
}
#endif

#endif
