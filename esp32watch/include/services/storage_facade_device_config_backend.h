#ifndef SERVICES_STORAGE_FACADE_DEVICE_CONFIG_BACKEND_H
#define SERVICES_STORAGE_FACADE_DEVICE_CONFIG_BACKEND_H

#include <stdbool.h>
#include "services/device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply a durable device configuration update through the storage facade backend path.
 *
 * @param[in] update Canonicalized update destined for durable commit.
 * @return true when the backend commit succeeded.
 * @throws None.
 */
bool storage_facade_device_config_backend_apply_update(const DeviceConfigUpdate *update);

/**
 * @brief Restore durable device configuration defaults through the storage facade backend path.
 *
 * @return true when defaults were committed.
 * @throws None.
 */
bool storage_facade_device_config_backend_restore_defaults(void);

#ifdef __cplusplus
}
#endif

#endif
