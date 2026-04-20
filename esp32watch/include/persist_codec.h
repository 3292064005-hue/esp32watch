#ifndef PERSIST_CODEC_H
#define PERSIST_CODEC_H

#include <stdbool.h>
#include <stdint.h>
#include "persist.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PERSIST_CAL_VALID_FLAG 0x1000U

#define PERSIST_GAME_STATS_VERSION_KEY "version"
#define PERSIST_GAME_BREAKOUT_KEY "breakout"
#define PERSIST_GAME_DINO_KEY "dino"
#define PERSIST_GAME_PONG_KEY "pong"
#define PERSIST_GAME_SNAKE_KEY "snake"
#define PERSIST_GAME_TETRIS_KEY "tetris"
#define PERSIST_GAME_SHOOTER_KEY "shooter"

#define PERSIST_SETTINGS_RECORD_KEY "settings"
#define PERSIST_ALARMS_RECORD_KEY "alarms"
#define PERSIST_CALIBRATION_RECORD_KEY "calibration"
#define PERSIST_SETTINGS_RECORD_MAGIC 0x53455453UL
#define PERSIST_ALARMS_RECORD_MAGIC 0x414C524DUL
#define PERSIST_CALIBRATION_RECORD_MAGIC 0x43414C42UL

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t crc;
    uint16_t settings0;
    uint16_t settings1;
    uint16_t settings2;
} DurableSettingsRecord;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t crc;
    uint16_t alarm0;
    uint16_t alarm1;
    uint16_t alarm2;
    uint16_t alarm_meta;
} DurableAlarmsRecord;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t crc;
    uint8_t valid;
    int16_t az_bias;
    int16_t gx_bias;
    int16_t gy_bias;
    int16_t gz_bias;
} DurableCalibrationRecord;

uint16_t persist_codec_calc_crc_raw(uint16_t settings0,
                                    uint16_t settings1,
                                    uint16_t settings2,
                                    uint16_t alarm0,
                                    uint16_t alarm1,
                                    uint16_t alarm2,
                                    uint16_t alarm_meta,
                                    uint16_t cal0,
                                    uint16_t cal1,
                                    uint16_t cal2,
                                    uint16_t cal3);

bool persist_codec_settings_record_valid(const DurableSettingsRecord *record, uint32_t magic, uint16_t version);
bool persist_codec_alarms_record_valid(const DurableAlarmsRecord *record, uint32_t magic, uint16_t version);
bool persist_codec_calibration_record_valid(const DurableCalibrationRecord *record, uint32_t magic, uint16_t version);

DurableSettingsRecord persist_codec_build_settings_record(const SettingsState *settings, uint32_t magic, uint16_t version);
DurableAlarmsRecord persist_codec_build_alarms_record(const AlarmState *alarms, uint8_t count, uint32_t magic, uint16_t version);
DurableCalibrationRecord persist_codec_build_calibration_record(const SensorCalibrationData *cal, uint32_t magic, uint16_t version);

#ifdef __cplusplus
}
#endif

#endif
