#include "services/storage_service_internal.h"
#include "app_config.h"
#include "persist.h"
#include "persist_flash.h"
#include <string.h>

typedef struct {
    bool active;
    bool use_flash;
    StorageBackendCommitPhase phase;
    SettingsState settings;
    AlarmState alarms[APP_MAX_ALARMS];
    SensorCalibrationData calibration;
    GameStatsState game_stats;
} StorageBackendCommitContext;

static StorageBackendCommitContext g_backend_commit;

static void storage_backend_adapter_reset_commit_context(void)
{
    memset(&g_backend_commit, 0, sizeof(g_backend_commit));
    g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_IDLE;
}

bool storage_backend_adapter_flash_ready(void)
{
#if APP_STORAGE_USE_FLASH
    return persist_flash_is_initialized();
#else
    return false;
#endif
}

bool storage_backend_adapter_bkp_ready(void)
{
    return persist_is_initialized();
}

bool storage_backend_adapter_app_state_durable_ready(void)
{
    return persist_app_state_durable_ready();
}

bool storage_backend_adapter_is_initialized(void)
{
    return storage_backend_adapter_flash_ready() || storage_backend_adapter_bkp_ready() || storage_backend_adapter_app_state_durable_ready();
}

uint8_t storage_backend_adapter_get_version(void)
{
    if (storage_backend_adapter_flash_ready()) {
        return persist_flash_get_version();
    }
    return persist_get_version();
}

StorageBackendType storage_backend_adapter_get_backend(void)
{
    return storage_backend_adapter_flash_ready() ? STORAGE_BACKEND_FLASH : STORAGE_BACKEND_BKP;
}

const char *storage_backend_adapter_get_backend_name(void)
{
    if (storage_backend_adapter_flash_ready()) {
        return "FLASH_JOURNAL";
    }
    if (storage_backend_adapter_app_state_durable_ready()) {
        return persist_app_state_backend_name();
    }
    return "RTC_RESET_DOMAIN";
}

uint16_t storage_backend_adapter_get_stored_crc(void)
{
    if (storage_backend_adapter_flash_ready()) {
        return persist_flash_get_stored_crc();
    }
    return persist_get_stored_crc();
}

uint16_t storage_backend_adapter_get_calculated_crc(void)
{
    if (storage_backend_adapter_flash_ready()) {
        return persist_flash_get_calculated_crc();
    }
    return persist_get_calculated_crc();
}

uint32_t storage_backend_adapter_get_wear_count(void)
{
#if APP_STORAGE_USE_FLASH
    return storage_backend_adapter_flash_ready() ? persist_flash_get_wear_count() : 0U;
#else
    return 0U;
#endif
}

void storage_backend_adapter_load_settings(SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
    if (storage_backend_adapter_flash_ready()) {
        persist_flash_load_settings(settings);
    } else {
        persist_load_settings(settings);
    }
}

void storage_backend_adapter_load_alarms(AlarmState *alarms, uint8_t count)
{
    if (alarms == NULL) {
        return;
    }
    if (storage_backend_adapter_flash_ready()) {
        persist_flash_load_alarms(alarms, count);
    } else {
        persist_load_alarms(alarms, count);
    }
}

void storage_backend_adapter_load_calibration(SensorCalibrationData *cal)
{
    if (cal == NULL) {
        return;
    }
    if (storage_backend_adapter_flash_ready()) {
        persist_flash_load_sensor_calibration(cal);
    } else {
        persist_load_sensor_calibration(cal);
    }
}

void storage_backend_adapter_load_game_stats(GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }
    if (storage_backend_adapter_flash_ready()) {
        persist_flash_load_game_stats(stats);
    } else {
        persist_load_game_stats(stats);
    }
}

bool storage_backend_adapter_begin_commit(const SettingsState *settings,
                                          const AlarmState *alarms,
                                          uint8_t count,
                                          const SensorCalibrationData *cal,
                                          const GameStatsState *stats)
{
    if (settings == NULL || alarms == NULL || cal == NULL || stats == NULL || g_backend_commit.active) {
        return false;
    }
    if (count > APP_MAX_ALARMS) {
        count = APP_MAX_ALARMS;
    }

    storage_backend_adapter_reset_commit_context();
    g_backend_commit.active = true;
    g_backend_commit.use_flash = storage_backend_adapter_flash_ready();
    g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_PREPARE;
    g_backend_commit.settings = *settings;
    memcpy(g_backend_commit.alarms, alarms, sizeof(AlarmState) * count);
    g_backend_commit.calibration = *cal;
    g_backend_commit.game_stats = *stats;

#if APP_STORAGE_USE_FLASH
    if (g_backend_commit.use_flash) {
        if (!persist_flash_begin_commit(&g_backend_commit.settings,
                                        g_backend_commit.alarms,
                                        APP_MAX_ALARMS,
                                        &g_backend_commit.calibration,
                                        &g_backend_commit.game_stats)) {
            storage_backend_adapter_reset_commit_context();
            return false;
        }
        g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_FLASH;
        return true;
    }
#endif

    g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_BKP_MIRROR;
    return true;
}

StorageBackendCommitTickResult storage_backend_adapter_commit_tick(void)
{
    if (!g_backend_commit.active) {
        return STORAGE_BACKEND_COMMIT_TICK_DONE_FAIL;
    }

#if APP_STORAGE_USE_FLASH
    if (g_backend_commit.use_flash) {
        PersistFlashCommitStepResult result = persist_flash_commit_step();
        if (result == PERSIST_FLASH_COMMIT_STEP_IN_PROGRESS) {
            g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_FLASH;
            return STORAGE_BACKEND_COMMIT_TICK_IN_PROGRESS;
        }
        if (result == PERSIST_FLASH_COMMIT_STEP_DONE_FAIL) {
            storage_backend_adapter_reset_commit_context();
            return STORAGE_BACKEND_COMMIT_TICK_DONE_FAIL;
        }
#if APP_STORAGE_MIRROR_BKP_WHEN_FLASH
        g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_BKP_MIRROR;
#else
        g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_VERIFY;
#endif
    }
#endif

    if (g_backend_commit.phase == STORAGE_BACKEND_COMMIT_PHASE_BKP_MIRROR) {
#if APP_STORAGE_MIRROR_BKP_WHEN_FLASH
        persist_save_settings(&g_backend_commit.settings);
        persist_save_alarms(g_backend_commit.alarms, APP_MAX_ALARMS);
        persist_save_sensor_calibration(&g_backend_commit.calibration);
        persist_save_game_stats(&g_backend_commit.game_stats);
#elif !APP_STORAGE_USE_FLASH
        persist_save_settings(&g_backend_commit.settings);
        persist_save_alarms(g_backend_commit.alarms, APP_MAX_ALARMS);
        persist_save_sensor_calibration(&g_backend_commit.calibration);
        persist_save_game_stats(&g_backend_commit.game_stats);
#endif
        g_backend_commit.phase = STORAGE_BACKEND_COMMIT_PHASE_VERIFY;
        return STORAGE_BACKEND_COMMIT_TICK_IN_PROGRESS;
    }

    if (g_backend_commit.phase == STORAGE_BACKEND_COMMIT_PHASE_VERIFY) {
        storage_backend_adapter_reset_commit_context();
        return STORAGE_BACKEND_COMMIT_TICK_DONE_OK;
    }

    return STORAGE_BACKEND_COMMIT_TICK_IN_PROGRESS;
}

StorageBackendCommitPhase storage_backend_adapter_commit_phase(void)
{
    return g_backend_commit.phase;
}

const char *storage_backend_adapter_commit_phase_name(StorageBackendCommitPhase phase)
{
    switch (phase) {
        case STORAGE_BACKEND_COMMIT_PHASE_PREPARE: return "PREP";
        case STORAGE_BACKEND_COMMIT_PHASE_FLASH: return "FLASH";
        case STORAGE_BACKEND_COMMIT_PHASE_BKP_MIRROR: return "NVS_MIRROR";
        case STORAGE_BACKEND_COMMIT_PHASE_VERIFY: return "VERIFY";
        default: return "IDLE";
    }
}

void storage_backend_adapter_abort_commit(void)
{
#if APP_STORAGE_USE_FLASH
    persist_flash_abort_commit();
#endif
    storage_backend_adapter_reset_commit_context();
}

bool storage_backend_adapter_commit_all(const SettingsState *settings,
                                        const AlarmState *alarms,
                                        uint8_t count,
                                        const SensorCalibrationData *cal,
                                        const GameStatsState *stats)
{
    if (!storage_backend_adapter_begin_commit(settings, alarms, count, cal, stats)) {
        return false;
    }

    for (uint8_t guard = 0U; guard < 16U; ++guard) {
        StorageBackendCommitTickResult result = storage_backend_adapter_commit_tick();
        if (result == STORAGE_BACKEND_COMMIT_TICK_DONE_OK) {
            return true;
        }
        if (result == STORAGE_BACKEND_COMMIT_TICK_DONE_FAIL) {
            return false;
        }
    }

    storage_backend_adapter_abort_commit();
    return false;
}

void storage_backend_adapter_capture_shadows(StorageServiceState *state)
{
    if (state == NULL || !storage_backend_adapter_is_initialized()) {
        return;
    }

    storage_backend_adapter_load_settings(&state->shadow_settings);
    storage_backend_adapter_load_alarms(state->shadow_alarms, APP_MAX_ALARMS);
    storage_backend_adapter_load_calibration(&state->shadow_calibration);
    storage_backend_adapter_load_game_stats(&state->shadow_game_stats);
    state->shadow_settings_valid = true;
    state->shadow_alarms_valid = true;
    state->shadow_calibration_valid = true;
    state->shadow_game_stats_valid = true;
}

bool storage_backend_adapter_migrate_from_bkp_to_flash(void)
{
#if APP_STORAGE_USE_FLASH
    if (!persist_flash_is_initialized() && persist_is_initialized()) {
        SettingsState settings = {0};
        AlarmState alarms[APP_MAX_ALARMS] = {0};
        SensorCalibrationData cal = {0};
        GameStatsState stats = {0};

        /*
         * Migration runs specifically when flash storage is not yet initialized.
         * In that state, routing through the generic backend adapter would choose
         * the BKP backend again, which would make migration a no-op. Commit the
         * BKP snapshot directly into the flash journal, then re-initialize flash
         * so the runtime shadow adopts the newly written record.
         */
        persist_load_settings(&settings);
        persist_load_alarms(alarms, APP_MAX_ALARMS);
        persist_load_sensor_calibration(&cal);
        persist_load_game_stats(&stats);
        if (!persist_flash_commit_all(&settings, alarms, APP_MAX_ALARMS, &cal, &stats)) {
            return false;
        }
        persist_flash_init();
    }
#endif
    return true;
}
