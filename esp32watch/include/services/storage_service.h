#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "app_limits.h"
#include "model.h"
#include "sensor.h"

typedef enum {
    STORAGE_COMMIT_STATE_IDLE = 0,
    STORAGE_COMMIT_STATE_STAGED,
    STORAGE_COMMIT_STATE_REQUESTED,
    STORAGE_COMMIT_STATE_FLUSH_BEFORE_SLEEP,
    STORAGE_COMMIT_STATE_TRANSACTION
} StorageCommitState;

typedef struct {
    uint32_t flash_wear_count;
    uint32_t commit_count;
    bool flash_backend_active;
} StorageWearStats;

/**
 * @brief Initialize storage backends, migration state and shadow copies.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void storage_service_init(void);
bool storage_service_is_initialized(void);
uint8_t storage_service_get_version(void);
StorageBackendType storage_service_get_backend(void);
const char *storage_service_get_backend_name(void);
bool storage_service_is_crc_valid(void);
uint16_t storage_service_get_stored_crc(void);
uint16_t storage_service_get_calculated_crc(void);
/**
 * @brief Advance the storage scheduler and execute any due commit.
 *
 * @param[in] now_ms Monotonic tick in milliseconds.
 * @return void
 * @throws None.
 */
void storage_service_tick(uint32_t now_ms);
bool storage_service_has_pending(void);
uint8_t storage_service_get_pending_mask(void);
uint32_t storage_service_get_commit_count(void);
uint32_t storage_service_get_last_commit_ms(void);
uint8_t storage_service_get_dirty_source_mask(void);
bool storage_service_get_last_commit_ok(void);
bool storage_service_is_transaction_active(void);
void storage_service_begin_transaction(void);
void storage_service_finalize_transaction(bool commit, StorageCommitReason reason);
void storage_service_abort_transaction(void);
uint32_t storage_service_get_wear_count(void);
void storage_service_get_wear_stats(StorageWearStats *out);
bool storage_service_recover_from_incomplete_commit(void);
StorageCommitReason storage_service_get_last_commit_reason(void);
StorageCommitState storage_service_get_commit_state(void);
const char *storage_service_commit_state_name(StorageCommitState state);
void storage_service_request_commit(StorageCommitReason reason);
void storage_service_request_flush_before_sleep(void);
bool storage_service_is_sleep_flush_pending(void);
void storage_service_commit_now(void);
/**
 * @brief Force an immediate commit using the provided reason tag.
 *
 * @param[in] reason Commit reason recorded for diagnostics and runtime observability.
 * @return void
 * @throws None.
 */
void storage_service_commit_now_with_reason(StorageCommitReason reason);
/**
 * @brief Load settings from the staged snapshot or active backend.
 *
 * @param[out] settings Destination buffer for settings data.
 * @return void
 * @throws None.
 */
void storage_service_load_settings(SettingsState *settings);
/**
 * @brief Stage settings data for a later coordinated commit.
 *
 * @param[in] settings Source settings data to stage.
 * @return void
 * @throws None.
 */
void storage_service_save_settings(const SettingsState *settings);
/**
 * @brief Load alarm state from the staged snapshot or active backend.
 *
 * @param[out] alarms Destination array for alarm entries.
 * @param[in] count Number of alarm entries requested.
 * @return void
 * @throws None.
 */
void storage_service_load_alarms(AlarmState *alarms, uint8_t count);
/**
 * @brief Stage alarm state for a later coordinated commit.
 *
 * @param[in] alarms Source alarm array to stage.
 * @param[in] count Number of alarm entries provided.
 * @return void
 * @throws None.
 */
void storage_service_save_alarms(const AlarmState *alarms, uint8_t count);
/**
 * @brief Load sensor calibration data from staged state or persistent backend.
 *
 * @param[out] cal Destination for calibration data.
 * @return void
 * @throws None.
 */
void storage_service_load_sensor_calibration(SensorCalibrationData *cal);
/**
 * @brief Stage sensor calibration data for a later coordinated commit.
 *
 * @param[in] cal Source calibration payload.
 * @return void
 * @throws None.
 */
void storage_service_save_sensor_calibration(const SensorCalibrationData *cal);
void storage_service_clear_sensor_calibration(void);

#endif
