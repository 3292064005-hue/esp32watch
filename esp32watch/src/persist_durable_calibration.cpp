#include <Preferences.h>
#include <string.h>

extern "C" {
#include "persist.h"
#include "main.h"
#include "app_config.h"
#include "board_features.h"
#include "common/storage_codec.h"
#include "persist_namespace_config.h"
}
#include "persist_preferences.h"
#include "persist_codec.h"
#include "persist_internal.hpp"

static bool persist_calibration_open(bool read_only)
{
    return persist_preferences_begin(g_calibration_prefs, PERSIST_PREFS_DOMAIN_SENSOR_CALIBRATION, read_only);
}

void rtc_mirror_calibration(const SensorCalibrationData *cal)
{
    uint16_t meta = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & (uint16_t)(~PERSIST_CAL_VALID_FLAG);
    int16_t az_bias = 0;
    int16_t gx_bias = 0;
    int16_t gy_bias = 0;
    int16_t gz_bias = 0;
    if (cal != NULL) {
        if (cal->valid) {
            meta |= PERSIST_CAL_VALID_FLAG;
        }
        az_bias = (int16_t)cal->az_bias;
        gx_bias = (int16_t)cal->gx_bias;
        gy_bias = (int16_t)cal->gy_bias;
        gz_bias = (int16_t)cal->gz_bias;
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM_META, meta);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL0, (uint16_t)az_bias);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL1, (uint16_t)gx_bias);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL2, (uint16_t)gy_bias);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL3, (uint16_t)gz_bias);
}

bool durable_store_calibration(const SensorCalibrationData *cal)
{
    if (!durable_app_state_enabled()) {
        return false;
    }
    DurableCalibrationRecord record = persist_codec_build_calibration_record(cal, PERSIST_CALIBRATION_RECORD_MAGIC, APP_STORAGE_VERSION);
    if (!persist_calibration_open(false)) {
        return false;
    }
    const bool ok = g_calibration_prefs.putBytes(PERSIST_CALIBRATION_RECORD_KEY, &record, sizeof(record)) == sizeof(record);
    g_calibration_prefs.end();
    rtc_mirror_calibration(cal);
    return ok;
}

bool durable_load_calibration(SensorCalibrationData *cal)
{
    DurableCalibrationRecord record = {};
    if (!durable_app_state_enabled() || cal == NULL) {
        return false;
    }
    if (!persist_calibration_open(true)) {
        return false;
    }
    const bool loaded = g_calibration_prefs.getBytes(PERSIST_CALIBRATION_RECORD_KEY, &record, sizeof(record)) == sizeof(record) &&
                        persist_codec_calibration_record_valid(&record, PERSIST_CALIBRATION_RECORD_MAGIC, APP_STORAGE_VERSION);
    g_calibration_prefs.end();
    if (!loaded) {
        return false;
    }
    memset(cal, 0, sizeof(*cal));
    cal->valid = record.valid != 0U;
    cal->az_bias = record.az_bias;
    cal->gx_bias = record.gx_bias;
    cal->gy_bias = record.gy_bias;
    cal->gz_bias = record.gz_bias;
    rtc_mirror_calibration(cal);
    return true;
}
