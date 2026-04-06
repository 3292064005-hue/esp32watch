#ifndef PERSIST_FLASH_INTERNAL_H
#define PERSIST_FLASH_INTERNAL_H

#include "persist_flash.h"
#include "storage_schema.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_STORAGE_MAGIC        0xA55AF10CUL
#define FLASH_RECORD_STATE_ERASED  0xFFFFU
#define FLASH_RECORD_STATE_RX      0xEEEEU
#define FLASH_RECORD_STATE_VALID   0xAAAAU

typedef StorageSchemaPayload FlashStoragePayload;

typedef struct {
    uint16_t state;
    uint16_t version;
    uint32_t magic;
    uint32_t sequence;
    uint16_t length;
    uint16_t crc;
    FlashStoragePayload payload;
} FlashStorageRecord;

typedef struct {
    uint32_t address;
    uint32_t sequence;
    bool valid;
    bool receive;
    uint16_t stored_crc;
    uint16_t calc_crc;
    FlashStoragePayload payload;
} FlashPageInfo;

void persist_flash_seed_default_settings(SettingsState *settings);
void persist_flash_seed_default_alarm(AlarmState *alarm, uint8_t index);
void persist_flash_payload_from_runtime(FlashStoragePayload *payload,
                                        const SettingsState *settings,
                                        const AlarmState *alarms,
                                        const SensorCalibrationData *cal);
void persist_flash_payload_to_settings(const FlashStoragePayload *payload, SettingsState *settings);
void persist_flash_payload_to_alarms(const FlashStoragePayload *payload, AlarmState *alarms, uint8_t count);
void persist_flash_payload_to_calibration(const FlashStoragePayload *payload, SensorCalibrationData *cal);
bool persist_flash_validate_record(const FlashStorageRecord *rec, FlashPageInfo *info);

#ifdef __cplusplus
}
#endif

#endif
