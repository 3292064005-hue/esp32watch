#ifndef SERVICES_STORAGE_FACADE_H
#define SERVICES_STORAGE_FACADE_H

#include "services/storage_service.h"
#include "services/storage_semantics.h"
#include "services/device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char backend[24];
    char backend_phase[24];
    char commit_state[24];
    uint8_t schema_version;
    bool flash_supported;
    bool flash_ready;
    bool migration_attempted;
    bool migration_ok;
    bool transaction_active;
    bool sleep_flush_pending;
    char app_state_backend[24];
    char device_config_backend[24];
    char app_state_durability[20];
    char device_config_durability[20];
    bool app_state_power_loss_guaranteed;
    bool device_config_power_loss_guaranteed;
    bool app_state_mixed_durability;
    uint8_t app_state_reset_domain_object_count;
    uint8_t app_state_durable_object_count;
} StorageFacadeRuntimeSnapshot;

/**
 * @brief Load runtime-owned settings through the app-state storage facade.
 *
 * @param[out] settings Destination settings buffer.
 * @return void
 * @throws None.
 */
void storage_facade_load_settings(SettingsState *settings);

/**
 * @brief Stage runtime-owned settings through the app-state storage facade.
 *
 * @param[in] settings Source settings state.
 * @return void
 * @throws None.
 */
void storage_facade_save_settings(const SettingsState *settings);

/**
 * @brief Load runtime-owned alarms through the app-state storage facade.
 *
 * @param[out] alarms Destination alarm array.
 * @param[in] count Alarm entry count.
 * @return void
 * @throws None.
 */
void storage_facade_load_alarms(AlarmState *alarms, uint8_t count);

/**
 * @brief Stage runtime-owned alarms through the app-state storage facade.
 *
 * @param[in] alarms Source alarm array.
 * @param[in] count Alarm entry count.
 * @return void
 * @throws None.
 */
void storage_facade_save_alarms(const AlarmState *alarms, uint8_t count);

/**
 * @brief Load runtime-owned game statistics through the app-state storage facade.
 *
 * @param[out] stats Destination game-stats buffer.
 * @return void
 * @throws None.
 */
void storage_facade_load_game_stats(GameStatsState *stats);

/**
 * @brief Stage runtime-owned game statistics through the app-state storage facade.
 *
 * @param[in] stats Source game-stats buffer.
 * @return void
 * @throws None.
 */
void storage_facade_save_game_stats(const GameStatsState *stats);

/**
 * @brief Load runtime-owned sensor calibration through the app-state storage facade.
 *
 * @param[out] cal Destination calibration buffer.
 * @return void
 * @throws None.
 */
void storage_facade_load_sensor_calibration(SensorCalibrationData *cal);

/**
 * @brief Stage runtime-owned sensor calibration through the app-state storage facade.
 *
 * @param[in] cal Source calibration buffer.
 * @return void
 * @throws None.
 */
void storage_facade_save_sensor_calibration(const SensorCalibrationData *cal);

/**
 * @brief Clear runtime-owned sensor calibration through the app-state storage facade.
 *
 * @return void
 * @throws None.
 */
void storage_facade_clear_sensor_calibration(void);

/**
 * @brief Initialize durable device-configuration persistence through the storage facade.
 *
 * @return void
 * @throws None.
 */
void storage_facade_device_config_init(void);

/**
 * @brief Read the current durable device configuration snapshot through the storage facade.
 *
 * @param[out] out Destination configuration snapshot.
 * @return true when @p out was populated.
 * @throws None.
 */
bool storage_facade_get_device_config(DeviceConfigSnapshot *out);

/**
 * @brief Read the persisted Wi-Fi password through the storage facade.
 *
 * @param[out] out Destination password buffer.
 * @param[in] out_size Destination buffer size.
 * @return true when the password buffer was populated.
 * @throws None.
 */
bool storage_facade_get_device_wifi_password(char *out, uint32_t out_size);

/**
 * @brief Read the persisted API token through the storage facade.
 *
 * @param[out] out Destination token buffer.
 * @param[in] out_size Destination buffer size.
 * @return true when the token buffer was populated.
 * @throws None.
 */
bool storage_facade_get_device_api_token(char *out, uint32_t out_size);

/**
 * @brief Apply a staged durable device configuration update through the storage facade.
 *
 * @param[in] update Requested configuration update.
 * @return true when the update validated and committed successfully.
 * @throws None.
 */
bool storage_facade_apply_device_config_update(const DeviceConfigUpdate *update);

/**
 * @brief Report whether durable control-plane authentication is configured.
 *
 * @return true when a mutation token exists.
 * @throws None.
 */
bool storage_facade_device_config_has_api_token(void);

/**
 * @brief Authenticate a presented mutation token against the durable device config.
 *
 * @param[in] token Candidate token.
 * @return true when access is permitted.
 * @throws None.
 */
bool storage_facade_authenticate_device_token(const char *token);

/**
 * @brief Reset durable device configuration ownership to defaults through the storage facade.
 *
 * @return true when defaults were committed.
 * @throws None.
 */
bool storage_facade_restore_device_config_defaults(void);

/**
 * @brief Collect storage runtime metadata, backend labels, and durability guarantees through the facade.
 *
 * @param[out] out Destination runtime metadata snapshot.
 * @return true when @p out was populated.
 * @throws None.
 * @boundary_behavior Returns false for NULL @p out or when required storage semantic descriptions are unavailable.
 */
bool storage_facade_get_runtime_snapshot(StorageFacadeRuntimeSnapshot *out);

/**
 * @brief Describe the authoritative storage semantic for a runtime-owned storage object.
 *
 * @param[in] kind Storage object kind.
 * @param[out] out Semantic description buffer.
 * @return true when the object kind is known to the facade.
 * @throws None.
 */
bool storage_facade_describe(StorageObjectKind kind, StorageObjectSemantic *out);

#ifdef __cplusplus
}
#endif

#endif
