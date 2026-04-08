#ifndef STORAGE_SCHEMA_H
#define STORAGE_SCHEMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app_limits.h"
#include "app_tuning.h"
#include "model.h"
#include "sensor.h"

#define STORAGE_SCHEMA_VERSION_V4      4U
#define STORAGE_SCHEMA_VERSION_V5      5U
#define STORAGE_SCHEMA_VERSION_CURRENT APP_STORAGE_VERSION
#define STORAGE_SCHEMA_CAL_VALID_MAGIC 0xCA1BU

typedef struct {
    uint16_t settings0;
    uint16_t settings1;
    uint16_t alarms[APP_MAX_ALARMS];
    uint16_t alarm_meta;
    int16_t cal[4];
    uint16_t reserved[8];
} StorageSchemaPayloadV4;

typedef struct {
    uint16_t settings0;
    uint16_t settings1;
    uint16_t settings2;
    uint16_t alarms[APP_MAX_ALARMS];
    uint16_t alarm_meta;
    int16_t cal[4];
    uint16_t reserved[8];
} StorageSchemaPayloadV5;

typedef struct {
    uint16_t settings0;
    uint16_t settings1;
    uint16_t settings2;
    uint16_t alarms[APP_MAX_ALARMS];
    uint16_t alarm_meta;
    int16_t cal[4];
    uint16_t breakout_hi;
    uint16_t dino_hi;
    uint16_t pong_hi;
    uint16_t snake_hi;
    uint16_t tetris_hi;
    uint16_t shooter_hi;
    uint16_t reserved[2];
} StorageSchemaPayloadV6;

typedef StorageSchemaPayloadV6 StorageSchemaPayload;

size_t storage_schema_payload_size_for_version(uint8_t version);
bool storage_schema_upgrade_payload(uint8_t src_version,
                                    const void *src_payload,
                                    size_t src_len,
                                    StorageSchemaPayload *dst_payload);
bool storage_schema_validate_payload(const StorageSchemaPayload *payload);
void storage_schema_from_runtime(StorageSchemaPayload *payload,
                                 const SettingsState *settings,
                                 const AlarmState *alarms,
                                 const SensorCalibrationData *cal,
                                 const GameStatsState *game_stats);
void storage_schema_to_runtime(const StorageSchemaPayload *payload,
                               SettingsState *settings,
                               AlarmState *alarms,
                               uint8_t count,
                               SensorCalibrationData *cal,
                               GameStatsState *game_stats);

#endif
