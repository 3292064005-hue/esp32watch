#ifndef STORAGE_SERVICE_INTERNAL_H
#define STORAGE_SERVICE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "app_limits.h"
#include "model.h"
#include "sensor.h"
#include "services/storage_service.h"

#define STORAGE_PENDING_SETTINGS    0x01U
#define STORAGE_PENDING_ALARMS      0x02U
#define STORAGE_PENDING_CALIBRATION 0x04U
#define STORAGE_PENDING_GAME_STATS  0x08U

typedef enum {
    STORAGE_COMMIT_PHASE_IDLE = 0,
    STORAGE_COMMIT_PHASE_PREPARE,
    STORAGE_COMMIT_PHASE_WRITE,
    STORAGE_COMMIT_PHASE_FINALIZE
} StorageCommitPhase;

typedef enum {
    STORAGE_BACKEND_COMMIT_PHASE_IDLE = 0,
    STORAGE_BACKEND_COMMIT_PHASE_PREPARE,
    STORAGE_BACKEND_COMMIT_PHASE_FLASH,
    STORAGE_BACKEND_COMMIT_PHASE_BKP_MIRROR,
    STORAGE_BACKEND_COMMIT_PHASE_VERIFY
} StorageBackendCommitPhase;

typedef enum {
    STORAGE_BACKEND_COMMIT_TICK_IN_PROGRESS = 0,
    STORAGE_BACKEND_COMMIT_TICK_DONE_OK,
    STORAGE_BACKEND_COMMIT_TICK_DONE_FAIL
} StorageBackendCommitTickResult;

typedef struct {
    bool pending_settings;
    bool pending_alarms;
    bool pending_calibration;
    bool pending_game_stats;
    bool shadow_settings_valid;
    bool shadow_alarms_valid;
    bool shadow_calibration_valid;
    bool shadow_game_stats_valid;
    bool transaction_active;
    bool commit_in_progress;
    bool backend_degraded;
    bool verify_failed;
    bool flush_before_sleep;
    StorageCommitPhase commit_phase;
    StorageCommitReason requested_commit_reason;
    uint8_t dirty_source_mask;
    uint32_t dirty_since_ms;
    uint32_t last_change_ms;
    uint32_t last_commit_ms;
    uint32_t commit_count;
    bool last_commit_ok;
    StorageCommitReason last_commit_reason;
    StorageCommitReason transaction_reason;
    StorageCommitReason inflight_commit_reason;
    bool inflight_commit_ok;
    bool inflight_previously_transaction_active;
    bool migration_attempted;
    bool migration_succeeded;
    SettingsState settings;
    AlarmState alarms[APP_MAX_ALARMS];
    SensorCalibrationData calibration;
    GameStatsState game_stats;
    SettingsState inflight_settings;
    AlarmState inflight_alarms[APP_MAX_ALARMS];
    SensorCalibrationData inflight_calibration;
    GameStatsState inflight_game_stats;
    SettingsState shadow_settings;
    AlarmState shadow_alarms[APP_MAX_ALARMS];
    SensorCalibrationData shadow_calibration;
    GameStatsState shadow_game_stats;
} StorageServiceState;

typedef struct {
    bool previously_transaction_active;
} StorageCommitExecutionContext;

extern StorageServiceState g_storage;

bool storage_backend_adapter_flash_ready(void);
bool storage_backend_adapter_bkp_ready(void);
bool storage_backend_adapter_app_state_durable_ready(void);
bool storage_backend_adapter_is_initialized(void);
uint8_t storage_backend_adapter_get_version(void);
StorageBackendType storage_backend_adapter_get_backend(void);
const char *storage_backend_adapter_get_backend_name(void);
bool storage_backend_adapter_get_capabilities(StorageBackendCapabilities *out);
uint16_t storage_backend_adapter_get_stored_crc(void);
uint16_t storage_backend_adapter_get_calculated_crc(void);
uint32_t storage_backend_adapter_get_wear_count(void);
void storage_backend_adapter_load_settings(SettingsState *settings);
void storage_backend_adapter_load_alarms(AlarmState *alarms, uint8_t count);
void storage_backend_adapter_load_calibration(SensorCalibrationData *cal);
void storage_backend_adapter_load_game_stats(GameStatsState *stats);
bool storage_backend_adapter_commit_all(const SettingsState *settings,
                                        const AlarmState *alarms,
                                        uint8_t count,
                                        const SensorCalibrationData *cal,
                                        const GameStatsState *stats);
bool storage_backend_adapter_begin_commit(const SettingsState *settings,
                                          const AlarmState *alarms,
                                          uint8_t count,
                                          const SensorCalibrationData *cal,
                                          const GameStatsState *stats);
StorageBackendCommitTickResult storage_backend_adapter_commit_tick(void);
StorageBackendCommitPhase storage_backend_adapter_commit_phase(void);
const char *storage_backend_adapter_commit_phase_name(StorageBackendCommitPhase phase);
void storage_backend_adapter_abort_commit(void);
void storage_backend_adapter_capture_shadows(StorageServiceState *state);
bool storage_backend_adapter_migrate_from_bkp_to_flash(void);

uint8_t storage_scheduler_pending_mask(const StorageServiceState *state);
bool storage_scheduler_has_pending(const StorageServiceState *state);
void storage_scheduler_mark_dirty(StorageServiceState *state, uint32_t now_ms, uint8_t source_bit);
void storage_scheduler_remember_requested_reason(StorageServiceState *state, StorageCommitReason reason);
StorageCommitReason storage_scheduler_choose_due_reason(const StorageServiceState *state, uint32_t now_ms);
StorageCommitState storage_scheduler_commit_state(const StorageServiceState *state);

bool storage_recovery_verify_runtime_state(StorageServiceState *state);
bool storage_recovery_refresh_backend_state(StorageServiceState *state);

void storage_tx_begin(StorageServiceState *state);
void storage_tx_finalize(StorageServiceState *state, bool commit, StorageCommitReason reason);
void storage_tx_abort(StorageServiceState *state);
bool storage_tx_prepare_commit(StorageServiceState *state, StorageCommitReason reason, StorageCommitExecutionContext *ctx);
void storage_tx_finish_commit(StorageServiceState *state, const StorageCommitExecutionContext *ctx, bool ok);

#endif
