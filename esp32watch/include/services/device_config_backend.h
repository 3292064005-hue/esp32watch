#ifndef SERVICES_DEVICE_CONFIG_BACKEND_H
#define SERVICES_DEVICE_CONFIG_BACKEND_H

#include <stdbool.h>
#include "services/device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate and atomically commit the durable device configuration backend snapshot.
 *
 * @param[in] update Canonicalized durable configuration update.
 * @return true when validation and commit completed successfully.
 * @throws None.
 * @boundary_behavior Backend-only write surface used by device-config authority and storage facade internals.
 */
bool device_config_backend_apply_update(const DeviceConfigUpdate *update);

/**
 * @brief Commit the default unprovisioned device configuration snapshot to durable storage.
 *
 * @return true when defaults were committed successfully.
 * @throws None.
 * @boundary_behavior Backend-only reset surface used by device-config authority and storage facade internals.
 */
bool device_config_backend_restore_defaults(void);

#ifdef __cplusplus
}
#endif

#endif
