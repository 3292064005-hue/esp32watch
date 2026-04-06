#ifndef PERSIST_FLASH_H
#define PERSIST_FLASH_H

#include <stdbool.h>
#include <stdint.h>
#include "app_limits.h"
#include "model.h"
#include "sensor.h"

typedef enum {
    PERSIST_FLASH_COMMIT_IDLE = 0,
    PERSIST_FLASH_COMMIT_ERASE_TARGET,
    PERSIST_FLASH_COMMIT_PROGRAM_RECORD,
    PERSIST_FLASH_COMMIT_MARK_VALID,
    PERSIST_FLASH_COMMIT_ERASE_PREVIOUS,
    PERSIST_FLASH_COMMIT_VERIFY,
    PERSIST_FLASH_COMMIT_DONE_OK,
    PERSIST_FLASH_COMMIT_DONE_FAIL
} PersistFlashCommitPhase;

typedef enum {
    PERSIST_FLASH_COMMIT_STEP_IN_PROGRESS = 0,
    PERSIST_FLASH_COMMIT_STEP_DONE_OK,
    PERSIST_FLASH_COMMIT_STEP_DONE_FAIL
} PersistFlashCommitStepResult;

void persist_flash_init(void);
bool persist_flash_is_initialized(void);
uint8_t persist_flash_get_version(void);
uint16_t persist_flash_get_stored_crc(void);
uint16_t persist_flash_get_calculated_crc(void);
bool persist_flash_commit_all(const SettingsState *settings,
                              const AlarmState *alarms,
                              uint8_t count,
                              const SensorCalibrationData *cal);
/**
 * @brief Prepare a multi-tick flash commit journal entry.
 *
 * @param[in] settings Settings payload to persist. Must not be NULL.
 * @param[in] alarms Alarm payload to persist. Must not be NULL.
 * @param[in] count Number of alarm entries provided.
 * @param[in] cal Calibration payload to persist. Must not be NULL.
 * @return true when the staged commit context was prepared; false on invalid input or when another staged commit is already active.
 * @throws None.
 */
bool persist_flash_begin_commit(const SettingsState *settings,
                                const AlarmState *alarms,
                                uint8_t count,
                                const SensorCalibrationData *cal);
/**
 * @brief Advance the staged flash commit by one backend phase.
 *
 * @param None.
 * @return Commit step status describing whether the staged commit is still running, completed successfully, or failed.
 * @throws None.
 */
PersistFlashCommitStepResult persist_flash_commit_step(void);
PersistFlashCommitPhase persist_flash_commit_phase(void);
const char *persist_flash_commit_phase_name(PersistFlashCommitPhase phase);
void persist_flash_abort_commit(void);
void persist_flash_save_settings(const SettingsState *settings);
void persist_flash_load_settings(SettingsState *settings);
void persist_flash_save_alarms(const AlarmState *alarms, uint8_t count);
void persist_flash_load_alarms(AlarmState *alarms, uint8_t count);
void persist_flash_save_sensor_calibration(const SensorCalibrationData *cal);
void persist_flash_load_sensor_calibration(SensorCalibrationData *cal);
uint32_t persist_flash_get_wear_count(void);

#endif
