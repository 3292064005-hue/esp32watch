#include "services/storage_service.h"
#include "services/storage_service_internal.h"
#include "app_config.h"
#include "platform_api.h"
#include <string.h>

bool storage_service_has_pending(void)
{
    return storage_scheduler_has_pending(&g_storage);
}

uint8_t storage_service_get_pending_mask(void)
{
    return storage_scheduler_pending_mask(&g_storage);
}

uint8_t storage_service_get_dirty_source_mask(void)
{
    return g_storage.dirty_source_mask;
}

uint32_t storage_service_get_commit_count(void)
{
    return g_storage.commit_count;
}

uint32_t storage_service_get_last_commit_ms(void)
{
    return g_storage.last_commit_ms;
}

bool storage_service_get_last_commit_ok(void)
{
    return g_storage.last_commit_ok;
}

bool storage_service_is_transaction_active(void)
{
    return g_storage.transaction_active;
}

uint32_t storage_service_get_wear_count(void)
{
    return storage_backend_adapter_get_wear_count();
}

void storage_service_get_wear_stats(StorageWearStats *out)
{
    if (out == NULL) {
        return;
    }
    out->flash_wear_count = storage_service_get_wear_count();
    out->commit_count = g_storage.commit_count;
    out->flash_backend_active = storage_backend_adapter_flash_ready();
}

bool storage_service_recover_from_incomplete_commit(void)
{
    return storage_recovery_refresh_backend_state(&g_storage);
}

StorageCommitReason storage_service_get_last_commit_reason(void)
{
    return g_storage.last_commit_reason;
}

StorageCommitState storage_service_get_commit_state(void)
{
    return storage_scheduler_commit_state(&g_storage);
}

const char *storage_service_commit_state_name(StorageCommitState state)
{
    switch (state) {
        case STORAGE_COMMIT_STATE_STAGED: return "STAGED";
        case STORAGE_COMMIT_STATE_REQUESTED: return "REQ";
        case STORAGE_COMMIT_STATE_FLUSH_BEFORE_SLEEP: return "SLEEP";
        case STORAGE_COMMIT_STATE_TRANSACTION: return "TX";
        default: return "IDLE";
    }
}

bool storage_service_is_initialized(void)
{
    return storage_backend_adapter_is_initialized();
}

uint8_t storage_service_get_version(void)
{
    return storage_backend_adapter_get_version();
}

uint8_t storage_service_get_schema_version(void)
{
    return APP_STORAGE_VERSION;
}

bool storage_service_flash_supported(void)
{
#if APP_STORAGE_USE_FLASH
    return true;
#else
    return false;
#endif
}

bool storage_service_is_flash_backend_ready(void)
{
    return storage_backend_adapter_flash_ready();
}

bool storage_service_app_state_durable_ready(void)
{
    return storage_backend_adapter_app_state_durable_ready() || storage_backend_adapter_flash_ready();
}

bool storage_service_was_migration_attempted(void)
{
    return g_storage.migration_attempted;
}

bool storage_service_last_migration_ok(void)
{
    return g_storage.migration_succeeded;
}

const char *storage_service_backend_phase_name(void)
{
    return storage_backend_adapter_commit_phase_name(storage_backend_adapter_commit_phase());
}

StorageBackendType storage_service_get_backend(void)
{
    return storage_backend_adapter_get_backend();
}

const char *storage_service_get_backend_name(void)
{
    return storage_backend_adapter_get_backend_name();
}

bool storage_service_is_crc_valid(void)
{
    return storage_service_get_stored_crc() == storage_service_get_calculated_crc();
}

uint16_t storage_service_get_stored_crc(void)
{
    return storage_backend_adapter_get_stored_crc();
}

uint16_t storage_service_get_calculated_crc(void)
{
    return storage_backend_adapter_get_calculated_crc();
}

void storage_service_load_settings(SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }

    if (g_storage.pending_settings) {
        *settings = g_storage.settings;
        return;
    }

    storage_backend_adapter_load_settings(settings);
}

void storage_service_save_settings(const SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }

    if (g_storage.pending_settings && memcmp(&g_storage.settings, settings, sizeof(*settings)) == 0) {
        return;
    }
    if (!g_storage.pending_settings && g_storage.shadow_settings_valid && memcmp(&g_storage.shadow_settings, settings, sizeof(*settings)) == 0) {
        return;
    }

    g_storage.settings = *settings;
    g_storage.pending_settings = true;
    storage_scheduler_mark_dirty(&g_storage, platform_time_now_ms(), STORAGE_PENDING_SETTINGS);
}

void storage_service_load_alarms(AlarmState *alarms, uint8_t count)
{
    if (alarms == NULL) {
        return;
    }
    if (count > APP_MAX_ALARMS) {
        count = APP_MAX_ALARMS;
    }

    if (g_storage.pending_alarms) {
        memcpy(alarms, g_storage.alarms, sizeof(AlarmState) * count);
        return;
    }

    storage_backend_adapter_load_alarms(alarms, count);
}

void storage_service_save_alarms(const AlarmState *alarms, uint8_t count)
{
    if (alarms == NULL) {
        return;
    }
    if (count > APP_MAX_ALARMS) {
        count = APP_MAX_ALARMS;
    }

    if (g_storage.pending_alarms && memcmp(g_storage.alarms, alarms, sizeof(AlarmState) * count) == 0) {
        return;
    }
    if (!g_storage.pending_alarms && g_storage.shadow_alarms_valid && memcmp(g_storage.shadow_alarms, alarms, sizeof(AlarmState) * count) == 0) {
        return;
    }

    memset(g_storage.alarms, 0, sizeof(g_storage.alarms));
    memcpy(g_storage.alarms, alarms, sizeof(AlarmState) * count);
    g_storage.pending_alarms = true;
    storage_scheduler_mark_dirty(&g_storage, platform_time_now_ms(), STORAGE_PENDING_ALARMS);
}

void storage_service_load_game_stats(GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }

    if (g_storage.pending_game_stats) {
        *stats = g_storage.game_stats;
        return;
    }

    storage_backend_adapter_load_game_stats(stats);
}

void storage_service_save_game_stats(const GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }

    if (g_storage.pending_game_stats && memcmp(&g_storage.game_stats, stats, sizeof(*stats)) == 0) {
        return;
    }
    if (!g_storage.pending_game_stats && g_storage.shadow_game_stats_valid &&
        memcmp(&g_storage.shadow_game_stats, stats, sizeof(*stats)) == 0) {
        return;
    }

    g_storage.game_stats = *stats;
    g_storage.pending_game_stats = true;
    storage_scheduler_mark_dirty(&g_storage, platform_time_now_ms(), STORAGE_PENDING_GAME_STATS);
}

void storage_service_load_sensor_calibration(SensorCalibrationData *cal)
{
    if (cal == NULL) {
        return;
    }

    if (g_storage.pending_calibration) {
        *cal = g_storage.calibration;
        return;
    }

    storage_backend_adapter_load_calibration(cal);
}

void storage_service_save_sensor_calibration(const SensorCalibrationData *cal)
{
    if (cal == NULL) {
        return;
    }

    if (g_storage.pending_calibration && memcmp(&g_storage.calibration, cal, sizeof(*cal)) == 0) {
        return;
    }
    if (!g_storage.pending_calibration && g_storage.shadow_calibration_valid && memcmp(&g_storage.shadow_calibration, cal, sizeof(*cal)) == 0) {
        return;
    }

    g_storage.calibration = *cal;
    g_storage.pending_calibration = true;
    storage_scheduler_mark_dirty(&g_storage, platform_time_now_ms(), STORAGE_PENDING_CALIBRATION);
}

void storage_service_clear_sensor_calibration(void)
{
    SensorCalibrationData cal = {0};
    cal.valid = false;
    storage_service_save_sensor_calibration(&cal);
}
