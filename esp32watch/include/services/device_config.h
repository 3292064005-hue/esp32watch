#ifndef SERVICES_DEVICE_CONFIG_H
#define SERVICES_DEVICE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_CONFIG_VERSION 1U
#define DEVICE_CONFIG_STORAGE_SCHEMA_VERSION 2U
#define DEVICE_CONFIG_WIFI_SSID_MAX_LEN 32U
#define DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN 63U
#define DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN 31U
#define DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN 31U
#define DEVICE_CONFIG_LOCATION_NAME_MAX_LEN 23U
#define DEVICE_CONFIG_API_TOKEN_MAX_LEN 32U

typedef struct {
    bool set_wifi;
    bool set_network;
    bool set_api_token;
    char wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX_LEN + 1U];
    char wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U];
    char timezone_posix[DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN + 1U];
    char timezone_id[DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN + 1U];
    char location_name[DEVICE_CONFIG_LOCATION_NAME_MAX_LEN + 1U];
    char api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U];
    float latitude;
    float longitude;
} DeviceConfigUpdate;

typedef struct {
    uint16_t version;
    bool wifi_configured;
    bool weather_configured;
    bool api_token_configured;
    char wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX_LEN + 1U];
    char timezone_posix[DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN + 1U];
    char timezone_id[DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN + 1U];
    char location_name[DEVICE_CONFIG_LOCATION_NAME_MAX_LEN + 1U];
    float latitude;
    float longitude;
} DeviceConfigSnapshot;

/**
 * @brief Initialize the persistent device configuration cache.
 *
 * @param None.
 * @return void
 * @throws None.
 * @boundary_behavior Safe to call repeatedly; subsequent calls reuse the in-memory cache.
 */
void device_config_init(void);

/**
 * @brief Read the current device configuration snapshot.
 *
 * @param[out] out Destination snapshot.
 * @return true when @p out was populated; false when @p out is NULL.
 * @throws None.
 */
bool device_config_get(DeviceConfigSnapshot *out);

/**
 * @brief Validate and atomically apply a staged configuration update.
 *
 * @param[in] update Requested field updates. Fields whose set_* flag is false keep their current persisted value.
 * @return true when the entire update validated and was committed; false when validation or persistence failed.
 * @throws None.
 * @boundary_behavior Uses a versioned double-slot commit record so a failed write cannot replace the last known-good committed snapshot.
 */
bool device_config_apply_update(const DeviceConfigUpdate *update);

/**
 * @brief Persist Wi-Fi station credentials.
 *
 * @param[in] ssid Station SSID. Pass an empty string to clear credentials.
 * @param[in] password Station password. Empty is allowed for open networks.
 * @return true when the configuration was validated and stored.
 * @throws None.
 * @boundary_behavior Rejects overlong fields and passwords whose length is between 1 and 7 bytes.
 */
bool device_config_save_wifi(const char *ssid, const char *password);

/**
 * @brief Persist time-sync and weather-sync configuration.
 *
 * @param[in] timezone_posix POSIX TZ string consumed by configTzTime().
 * @param[in] timezone_id IANA timezone identifier consumed by the weather provider.
 * @param[in] latitude Decimal latitude in the inclusive range [-90, 90].
 * @param[in] longitude Decimal longitude in the inclusive range [-180, 180].
 * @param[in] location_name Human-readable location label.
 * @return true when the configuration was validated and stored.
 * @throws None.
 */
bool device_config_save_network_profile(const char *timezone_posix,
                                        const char *timezone_id,
                                        float latitude,
                                        float longitude,
                                        const char *location_name);

/**
 * @brief Persist or clear the API mutation token.
 *
 * @param[in] token Token string. Pass an empty string to clear the token.
 * @return true when the token was validated and stored.
 * @throws None.
 * @boundary_behavior Rejects tokens longer than DEVICE_CONFIG_API_TOKEN_MAX_LEN.
 */
bool device_config_save_api_token(const char *token);

/**
 * @brief Copy the persisted Wi-Fi station password into a caller buffer.
 *
 * @param[out] out Destination buffer.
 * @param[in] out_size Destination buffer size in bytes.
 * @return true when the buffer was populated; false when the arguments are invalid.
 * @throws None.
 */
bool device_config_get_wifi_password(char *out, uint32_t out_size);

/**
 * @brief Copy the persisted API mutation token into a caller buffer.
 *
 * @param[out] out Destination buffer.
 * @param[in] out_size Destination buffer size in bytes.
 * @return true when the buffer was populated; false when the arguments are invalid.
 * @throws None.
 */
bool device_config_get_api_token(char *out, uint32_t out_size);

/**
 * @brief Report whether a mutation token is configured.
 *
 * @return true when an API token is configured; false otherwise.
 * @throws None.
 */
bool device_config_has_api_token(void);

/**
 * @brief Validate a presented mutation token against the persisted token.
 *
 * @param[in] token Candidate token from the caller.
 * @return true when no token is configured or the provided token matches exactly.
 * @throws None.
 */
bool device_config_authenticate_token(const char *token);

/**
 * @brief Reset the device configuration to a clean, unprovisioned state.
 *
 * @return true when the reset completed; false when persistence could not commit defaults.
 * @throws None.
 * @boundary_behavior Preserves the previous committed snapshot if the new default snapshot cannot be committed.
 */
bool device_config_restore_defaults(void);

/**
 * @brief Read the monotonic committed generation of the authoritative device configuration snapshot.
 *
 * @return Commit generation that increments after every durable device-config write. Returns 0 when the configuration has not been persisted yet.
 * @throws None.
 */
uint32_t device_config_generation(void);

/**
 * @brief Report the durable backend label used by device configuration persistence.
 *
 * @return Backend label string. Returns "UNAVAILABLE" when the backend could not be opened.
 * @throws None.
 */
const char *device_config_backend_name(void);

/**
 * @brief Report whether the durable device configuration backend is currently usable.
 *
 * @return true when the backing Preferences namespace opened successfully.
 * @throws None.
 */
bool device_config_backend_ready(void);

/**
 * @brief Report whether the most recent durable device configuration commit succeeded.
 *
 * @return true when the last attempted commit completed successfully, or when a valid snapshot was loaded.
 * @throws None.
 */
bool device_config_last_commit_ok(void);


#ifdef __cplusplus
}
#endif

#endif
