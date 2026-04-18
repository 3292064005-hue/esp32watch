#include "services/storage_semantics.h"
#include "services/storage_service.h"
#include <string.h>

static const StorageObjectSemantic kStorageSemanticTable[STORAGE_OBJECT_COUNT] = {
    {STORAGE_OBJECT_APP_SETTINGS, "APP_SETTINGS", "storage_service", "app.settings", STORAGE_DURABILITY_RESET_DOMAIN, STORAGE_RESET_BEHAVIOR_RESET_APP_STATE, true, false},
    {STORAGE_OBJECT_APP_ALARMS, "APP_ALARMS", "storage_service", "app.alarms", STORAGE_DURABILITY_RESET_DOMAIN, STORAGE_RESET_BEHAVIOR_RESET_APP_STATE, true, false},
    {STORAGE_OBJECT_SENSOR_CALIBRATION, "SENSOR_CAL", "storage_service", "sensor.calibration", STORAGE_DURABILITY_RESET_DOMAIN, STORAGE_RESET_BEHAVIOR_RESET_APP_STATE, true, false},
    {STORAGE_OBJECT_GAME_STATS, "GAME_STATS", "storage_service", "games.stats", STORAGE_DURABILITY_DURABLE, STORAGE_RESET_BEHAVIOR_FACTORY_RESET, true, true},
    {STORAGE_OBJECT_DEVICE_CONFIG, "DEVICE_CONFIG", "device_config", "device.config", STORAGE_DURABILITY_DURABLE, STORAGE_RESET_BEHAVIOR_RESET_DEVICE_CONFIG, false, true},
    {STORAGE_OBJECT_TIME_RECOVERY, "TIME_RECOVERY", "time_service", "time.recovery", STORAGE_DURABILITY_DURABLE, STORAGE_RESET_BEHAVIOR_NEVER_RESET, false, true},
};

StorageDurabilityLevel storage_service_runtime_durability_level(void)
{
    return storage_service_app_state_durable_ready() ? STORAGE_DURABILITY_DURABLE : STORAGE_DURABILITY_RESET_DOMAIN;
}

const char *storage_durability_level_name(StorageDurabilityLevel level)
{
    switch (level) {
        case STORAGE_DURABILITY_RESET_DOMAIN: return "RESET_DOMAIN";
        case STORAGE_DURABILITY_DURABLE: return "DURABLE";
        default: return "VOLATILE";
    }
}

const char *storage_reset_behavior_name(StorageResetBehavior behavior)
{
    switch (behavior) {
        case STORAGE_RESET_BEHAVIOR_RESET_APP_STATE: return "RESET_APP_STATE";
        case STORAGE_RESET_BEHAVIOR_RESET_DEVICE_CONFIG: return "RESET_DEVICE_CONFIG";
        case STORAGE_RESET_BEHAVIOR_FACTORY_RESET: return "FACTORY_RESET";
        default: return "NEVER_RESET";
    }
}

bool storage_semantics_describe(StorageObjectKind kind, StorageObjectSemantic *out)
{
    if (out == NULL || kind >= STORAGE_OBJECT_COUNT) {
        return false;
    }

    *out = kStorageSemanticTable[(uint32_t)kind];
    if (out->managed_by_storage_service && out->durability == STORAGE_DURABILITY_RESET_DOMAIN) {
        out->durability = storage_service_runtime_durability_level();
        out->survives_power_loss = out->durability == STORAGE_DURABILITY_DURABLE;
    }
    return true;
}

uint32_t storage_semantics_count(void)
{
    return STORAGE_OBJECT_COUNT;
}

bool storage_semantics_at(uint32_t index, StorageObjectSemantic *out)
{
    if (index >= STORAGE_OBJECT_COUNT) {
        return false;
    }
    return storage_semantics_describe((StorageObjectKind)index, out);
}
