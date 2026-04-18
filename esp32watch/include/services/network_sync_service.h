#ifndef NETWORK_SYNC_SERVICE_H
#define NETWORK_SYNC_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "services/device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool wifi_connected;
    bool time_synced;
    uint32_t last_time_sync_ms;
    bool weather_valid;
    int16_t temperature_tenths_c;
    uint8_t weather_code;
    char weather_text[20];
    char location_name[24];
    uint32_t last_weather_sync_ms;
    bool weather_tls_verified;
    bool weather_ca_loaded;
    bool weather_last_attempt_ok;
    int32_t last_weather_http_status;
    char weather_tls_mode[16];
    char weather_status[20];
} NetworkSyncSnapshot;

/**
 * @brief Initialize the network time/weather synchronization state machine.
 *
 * Seeds the runtime snapshot, reconciles persisted provisioning data, and prepares
 * the service for later Wi-Fi-driven synchronization attempts.
 *
 * @return true when the service initialized its internal snapshot successfully.
 * @throws None.
 */
bool network_sync_service_init(void);

/**
 * @brief Advance the network synchronization state machine.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return void
 * @throws None.
 * @boundary_behavior Returns early when Wi-Fi is offline; the latest successful snapshot remains available for diagnostics.
 */
void network_sync_service_tick(uint32_t now_ms);

/**
 * @brief Force the next tick to attempt immediate time and weather refreshes.
 *
 * @return void
 * @throws None.
 */
void network_sync_service_request_refresh(void);

/**
 * @brief Copy the latest synchronization snapshot.
 *
 * @param[out] out Destination snapshot.
 * @return true when @p out was populated; false when @p out is NULL.
 * @throws None.
 */
bool network_sync_service_get_snapshot(NetworkSyncSnapshot *out);
const char *network_sync_service_weather_text(uint8_t weather_code);

/**
 * @brief Verify that the authoritative device-configuration generation has been applied by the network sync service.
 *
 * @param[in] generation Authoritative device-config generation expected to be active.
 * @param[in] cfg Optional config snapshot used for semantic cross-checking.
 * @return true when the network-sync runtime state reflects the requested generation and profile values.
 * @throws None.
 */
bool network_sync_service_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg);

/**
 * @brief Return the last authoritative device-config generation applied by the network sync service.
 *
 * @return Generation number, or 0 when no config has been applied yet.
 * @throws None.
 */
uint32_t network_sync_service_last_applied_generation(void);

#if defined(HOST_RUNTIME_TEST)
void network_sync_service_test_seed_weather_state(uint32_t generation,
                                                  const char *location,
                                                  const char *status);
void network_sync_service_test_inject_weather_result(uint32_t generation,
                                                     bool success,
                                                     const char *location,
                                                     const char *status,
                                                     int16_t temperature_tenths_c,
                                                     uint8_t weather_code,
                                                     uint32_t completed_at_ms);
void network_sync_service_test_apply_pending_weather_result(void);
#endif


#ifdef __cplusplus
}
#endif

#endif
