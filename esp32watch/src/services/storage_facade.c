#include "services/storage_facade.h"
#include <string.h>

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static void storage_facade_ensure_device_config_ready(void)
{
    device_config_init();
}

static bool storage_facade_is_app_state_object(StorageObjectKind kind)
{
    return kind == STORAGE_OBJECT_APP_SETTINGS ||
           kind == STORAGE_OBJECT_APP_ALARMS ||
           kind == STORAGE_OBJECT_SENSOR_CALIBRATION ||
           kind == STORAGE_OBJECT_GAME_STATS;
}

static void storage_facade_collect_app_state_durability(StorageFacadeRuntimeSnapshot *out)
{
    StorageObjectSemantic semantic = {};

    if (out == NULL) {
        return;
    }

    for (uint32_t i = 0U; i < storage_semantics_count(); ++i) {
        if (!storage_semantics_at(i, &semantic) || !storage_facade_is_app_state_object(semantic.kind)) {
            continue;
        }
        if (semantic.durability == STORAGE_DURABILITY_DURABLE) {
            if (out->app_state_durable_object_count < 0xFFU) {
                ++out->app_state_durable_object_count;
            }
        } else {
            if (out->app_state_reset_domain_object_count < 0xFFU) {
                ++out->app_state_reset_domain_object_count;
            }
        }
    }

    out->app_state_mixed_durability = out->app_state_durable_object_count > 0U &&
                                      out->app_state_reset_domain_object_count > 0U;
    out->app_state_power_loss_guaranteed = out->app_state_reset_domain_object_count == 0U &&
                                           out->app_state_durable_object_count > 0U;
}


void storage_facade_load_settings(SettingsState *settings)
{
    storage_service_load_settings(settings);
}

void storage_facade_save_settings(const SettingsState *settings)
{
    storage_service_save_settings(settings);
}

void storage_facade_load_alarms(AlarmState *alarms, uint8_t count)
{
    storage_service_load_alarms(alarms, count);
}

void storage_facade_save_alarms(const AlarmState *alarms, uint8_t count)
{
    storage_service_save_alarms(alarms, count);
}

void storage_facade_load_game_stats(GameStatsState *stats)
{
    storage_service_load_game_stats(stats);
}

void storage_facade_save_game_stats(const GameStatsState *stats)
{
    storage_service_save_game_stats(stats);
}

void storage_facade_load_sensor_calibration(SensorCalibrationData *cal)
{
    storage_service_load_sensor_calibration(cal);
}

void storage_facade_save_sensor_calibration(const SensorCalibrationData *cal)
{
    storage_service_save_sensor_calibration(cal);
}

void storage_facade_clear_sensor_calibration(void)
{
    storage_service_clear_sensor_calibration();
}

void storage_facade_device_config_init(void)
{
    storage_facade_ensure_device_config_ready();
}

bool storage_facade_get_device_config(DeviceConfigSnapshot *out)
{
    storage_facade_ensure_device_config_ready();
    return device_config_get(out);
}

bool storage_facade_get_device_wifi_password(char *out, uint32_t out_size)
{
    storage_facade_ensure_device_config_ready();
    return device_config_get_wifi_password(out, out_size);
}

bool storage_facade_get_device_api_token(char *out, uint32_t out_size)
{
    storage_facade_ensure_device_config_ready();
    return device_config_get_api_token(out, out_size);
}

bool storage_facade_apply_device_config_update(const DeviceConfigUpdate *update)
{
    storage_facade_ensure_device_config_ready();
    return device_config_apply_update(update);
}

bool storage_facade_device_config_has_api_token(void)
{
    storage_facade_ensure_device_config_ready();
    return device_config_has_api_token();
}

bool storage_facade_authenticate_device_token(const char *token)
{
    storage_facade_ensure_device_config_ready();
    return device_config_authenticate_token(token);
}

bool storage_facade_restore_device_config_defaults(void)
{
    storage_facade_ensure_device_config_ready();
    return device_config_restore_defaults();
}

bool storage_facade_get_runtime_snapshot(StorageFacadeRuntimeSnapshot *out)
{
    StorageObjectSemantic semantic = {};

    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    copy_text(out->backend, sizeof(out->backend), storage_service_get_backend_name());
    copy_text(out->backend_phase, sizeof(out->backend_phase), storage_service_backend_phase_name());
    copy_text(out->commit_state,
              sizeof(out->commit_state),
              storage_service_commit_state_name(storage_service_get_commit_state()));
    out->schema_version = storage_service_get_schema_version();
    out->flash_supported = storage_service_flash_supported();
    out->flash_ready = storage_service_is_flash_backend_ready();
    out->migration_attempted = storage_service_was_migration_attempted();
    out->migration_ok = storage_service_last_migration_ok();
    out->transaction_active = storage_service_is_transaction_active();
    out->sleep_flush_pending = storage_service_is_sleep_flush_pending();
    copy_text(out->app_state_backend, sizeof(out->app_state_backend), storage_service_get_backend_name());
    copy_text(out->device_config_backend, sizeof(out->device_config_backend), device_config_backend_name());

    if (!storage_facade_describe(STORAGE_OBJECT_APP_SETTINGS, &semantic)) {
        return false;
    }
    copy_text(out->app_state_durability,
              sizeof(out->app_state_durability),
              storage_durability_level_name(semantic.durability));
    storage_facade_collect_app_state_durability(out);

    if (!storage_facade_describe(STORAGE_OBJECT_DEVICE_CONFIG, &semantic)) {
        return false;
    }
    copy_text(out->device_config_durability,
              sizeof(out->device_config_durability),
              storage_durability_level_name(semantic.durability));
    out->device_config_power_loss_guaranteed = semantic.survives_power_loss && device_config_backend_ready() && device_config_last_commit_ok();
    return true;
}

bool storage_facade_describe(StorageObjectKind kind, StorageObjectSemantic *out)
{
    return storage_semantics_describe(kind, out);
}
