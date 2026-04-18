#ifndef SERVICES_STORAGE_SEMANTICS_H
#define SERVICES_STORAGE_SEMANTICS_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STORAGE_DURABILITY_VOLATILE = 0,
    STORAGE_DURABILITY_RESET_DOMAIN,
    STORAGE_DURABILITY_DURABLE
} StorageDurabilityLevel;

typedef enum {
    STORAGE_RESET_BEHAVIOR_RESET_APP_STATE = 0,
    STORAGE_RESET_BEHAVIOR_RESET_DEVICE_CONFIG,
    STORAGE_RESET_BEHAVIOR_FACTORY_RESET,
    STORAGE_RESET_BEHAVIOR_NEVER_RESET
} StorageResetBehavior;

typedef enum {
    STORAGE_OBJECT_APP_SETTINGS = 0,
    STORAGE_OBJECT_APP_ALARMS,
    STORAGE_OBJECT_SENSOR_CALIBRATION,
    STORAGE_OBJECT_GAME_STATS,
    STORAGE_OBJECT_DEVICE_CONFIG,
    STORAGE_OBJECT_TIME_RECOVERY,
    STORAGE_OBJECT_COUNT
} StorageObjectKind;

typedef struct {
    StorageObjectKind kind;
    const char *name;
    const char *owner;
    const char *namespace_name;
    StorageDurabilityLevel durability;
    StorageResetBehavior reset_behavior;
    bool managed_by_storage_service;
    bool survives_power_loss;
} StorageObjectSemantic;

StorageDurabilityLevel storage_service_runtime_durability_level(void);
const char *storage_durability_level_name(StorageDurabilityLevel level);
const char *storage_reset_behavior_name(StorageResetBehavior behavior);
bool storage_semantics_describe(StorageObjectKind kind, StorageObjectSemantic *out);
uint32_t storage_semantics_count(void);
bool storage_semantics_at(uint32_t index, StorageObjectSemantic *out);

#ifdef __cplusplus
}
#endif

#endif
