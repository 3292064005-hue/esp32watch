#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "services/device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Wi-Fi connectivity and provisioning fallback.
 *
 * @return true when the Wi-Fi state machine was initialized; false on fatal setup failure.
 * @throws None.
 * @boundary_behavior When no STA credentials are configured, the module starts a provisioning AP instead of failing.
 */
bool web_wifi_init(void);

/**
 * @brief Advance the Wi-Fi state machine.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return void
 * @throws None.
 */
void web_wifi_tick(uint32_t now_ms);

bool web_wifi_connected(void);
bool web_wifi_transition_active(void);
const char *web_wifi_ip_string(void);
int32_t web_wifi_rssi(void);
bool web_wifi_access_point_active(void);
const char *web_wifi_access_point_ssid(void);
const char *web_wifi_access_point_password(void);
bool web_wifi_serial_password_log_enabled(void);
const char *web_wifi_mode_name(void);
const char *web_wifi_status_name(void);
bool web_wifi_has_network_route(void);

/**
 * @brief Verify that the authoritative device configuration generation has been applied to the Wi-Fi service state.
 *
 * @param[in] generation Authoritative device-config generation that should already be applied.
 * @param[in] cfg Optional authoritative config snapshot used to cross-check Wi-Fi mode/SSID semantics.
 * @return true when the Wi-Fi service has applied the requested generation and its externally visible state matches the authoritative config intent.
 * @throws None.
 */
bool web_wifi_verify_config_applied(uint32_t generation, const DeviceConfigSnapshot *cfg);

/**
 * @brief Return the last authoritative device-config generation applied by the Wi-Fi service.
 *
 * @return Generation number of the most recently applied Wi-Fi config. Returns 0 when no config has been applied yet.
 * @throws None.
 */
uint32_t web_wifi_last_applied_generation(void);

#ifdef __cplusplus
}
#endif
