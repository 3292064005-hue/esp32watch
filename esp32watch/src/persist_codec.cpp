#include <string.h>

extern "C" {
#include "persist_codec.h"
#include "app_config.h"
#include "common/storage_codec.h"
}

static uint16_t crc16_mix_local(uint16_t crc, uint16_t value)
{
    crc ^= value;
    for (uint8_t bit = 0; bit < 16U; ++bit) {
        crc = (crc & 1U) ? (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
    }
    return crc;
}

extern "C" uint16_t persist_codec_calc_crc_raw(uint16_t settings0,
                                                 uint16_t settings1,
                                                 uint16_t settings2,
                                                 uint16_t alarm0,
                                                 uint16_t alarm1,
                                                 uint16_t alarm2,
                                                 uint16_t alarm_meta,
                                                 uint16_t cal0,
                                                 uint16_t cal1,
                                                 uint16_t cal2,
                                                 uint16_t cal3)
{
    uint16_t crc = 0xFFFFU;
    crc = crc16_mix_local(crc, settings0);
    crc = crc16_mix_local(crc, settings1);
    crc = crc16_mix_local(crc, settings2);
    crc = crc16_mix_local(crc, alarm0);
    crc = crc16_mix_local(crc, alarm1);
    crc = crc16_mix_local(crc, alarm2);
    crc = crc16_mix_local(crc, alarm_meta);
    crc = crc16_mix_local(crc, cal0);
    crc = crc16_mix_local(crc, cal1);
    crc = crc16_mix_local(crc, cal2);
    crc = crc16_mix_local(crc, cal3);
    return crc;
}

extern "C" bool persist_codec_settings_record_valid(const DurableSettingsRecord *record, uint32_t magic, uint16_t version)
{
    if (record == NULL) {
        return false;
    }
    return record->magic == magic &&
           record->version == version &&
           record->crc == persist_codec_calc_crc_raw(record->settings0,
                                                     record->settings1,
                                                     record->settings2,
                                                     0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U);
}

extern "C" bool persist_codec_alarms_record_valid(const DurableAlarmsRecord *record, uint32_t magic, uint16_t version)
{
    if (record == NULL) {
        return false;
    }
    return record->magic == magic &&
           record->version == version &&
           record->crc == persist_codec_calc_crc_raw(0U,
                                                     0U,
                                                     0U,
                                                     record->alarm0,
                                                     record->alarm1,
                                                     record->alarm2,
                                                     record->alarm_meta,
                                                     0U, 0U, 0U, 0U);
}

extern "C" bool persist_codec_calibration_record_valid(const DurableCalibrationRecord *record, uint32_t magic, uint16_t version)
{
    if (record == NULL) {
        return false;
    }
    return record->magic == magic &&
           record->version == version &&
           record->crc == persist_codec_calc_crc_raw(0U,
                                                     0U,
                                                     0U,
                                                     0U,
                                                     0U,
                                                     0U,
                                                     record->valid ? PERSIST_CAL_VALID_FLAG : 0U,
                                                     (uint16_t)record->az_bias,
                                                     (uint16_t)record->gx_bias,
                                                     (uint16_t)record->gy_bias,
                                                     (uint16_t)record->gz_bias);
}

extern "C" DurableSettingsRecord persist_codec_build_settings_record(const SettingsState *settings, uint32_t magic, uint16_t version)
{
    DurableSettingsRecord record = {};
    if (settings == NULL) {
        return record;
    }
    record.magic = magic;
    record.version = version;
    record.settings0 = storage_pack_settings0(settings);
    record.settings1 = storage_pack_settings1(settings);
    record.settings2 = storage_pack_settings2(settings);
    record.crc = persist_codec_calc_crc_raw(record.settings0,
                                            record.settings1,
                                            record.settings2,
                                            0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U);
    return record;
}

extern "C" DurableAlarmsRecord persist_codec_build_alarms_record(const AlarmState *alarms, uint8_t count, uint32_t magic, uint16_t version)
{
    AlarmState local[APP_MAX_ALARMS] = {};
    DurableAlarmsRecord record = {};
    for (uint8_t i = 0U; i < APP_MAX_ALARMS && i < count; ++i) {
        local[i] = alarms[i];
    }
    record.magic = magic;
    record.version = version;
    record.alarm0 = storage_pack_alarm_word(&local[0]);
    record.alarm1 = storage_pack_alarm_word(&local[1]);
    record.alarm2 = storage_pack_alarm_word(&local[2]);
    record.alarm_meta = storage_pack_alarm_meta(&local[0], &local[1], &local[2]);
    record.crc = persist_codec_calc_crc_raw(0U, 0U, 0U,
                                            record.alarm0,
                                            record.alarm1,
                                            record.alarm2,
                                            record.alarm_meta,
                                            0U, 0U, 0U, 0U);
    return record;
}

extern "C" DurableCalibrationRecord persist_codec_build_calibration_record(const SensorCalibrationData *cal, uint32_t magic, uint16_t version)
{
    DurableCalibrationRecord record = {};
    record.magic = magic;
    record.version = version;
    record.valid = cal != NULL && cal->valid;
    record.az_bias = (int16_t)((cal != NULL) ? cal->az_bias : 0);
    record.gx_bias = (int16_t)((cal != NULL) ? cal->gx_bias : 0);
    record.gy_bias = (int16_t)((cal != NULL) ? cal->gy_bias : 0);
    record.gz_bias = (int16_t)((cal != NULL) ? cal->gz_bias : 0);
    record.crc = persist_codec_calc_crc_raw(0U, 0U, 0U, 0U, 0U, 0U,
                                            record.valid ? PERSIST_CAL_VALID_FLAG : 0U,
                                            (uint16_t)record.az_bias,
                                            (uint16_t)record.gx_bias,
                                            (uint16_t)record.gy_bias,
                                            (uint16_t)record.gz_bias);
    return record;
}
