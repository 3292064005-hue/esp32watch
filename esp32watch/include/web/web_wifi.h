#pragma once
#include <stdint.h>
#include <stdbool.h>

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

/**
 * @brief Re-read persisted configuration and restart Wi-Fi accordingly.
 *
 * @return true when the reconfiguration request was accepted; false when initialization has not run yet.
 * @throws None.
 */
bool web_wifi_reconfigure(void);

bool web_wifi_connected(void);
const char *web_wifi_ip_string(void);
int32_t web_wifi_rssi(void);
bool web_wifi_access_point_active(void);
const char *web_wifi_access_point_ssid(void);
const char *web_wifi_access_point_password(void);
const char *web_wifi_mode_name(void);
const char *web_wifi_status_name(void);
bool web_wifi_has_network_route(void);

#ifdef __cplusplus
}
#endif
