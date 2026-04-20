extern "C" {
#include "persist.h"
#include "main.h"
#include "app_config.h"
#include "persist_codec.h"
}

#include "persist_internal.hpp"

extern "C" void persist_save_sensor_calibration(const SensorCalibrationData *cal)
{
    (void)durable_store_calibration(cal);
    rtc_mirror_calibration(cal);
    write_crc();
}

extern "C" void persist_load_sensor_calibration(SensorCalibrationData *cal)
{
    if (cal == NULL) {
        return;
    }
    cal->valid = false;
    cal->az_bias = 0;
    cal->gx_bias = 0;
    cal->gy_bias = 0;
    cal->gz_bias = 0;
    if (durable_load_calibration(cal)) {
        return;
    }
    if (!persist_is_initialized()) {
        return;
    }
    cal->valid = (platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & PERSIST_CAL_VALID_FLAG) != 0U;
    if (!cal->valid) {
        return;
    }
    cal->az_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL0);
    cal->gx_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL1);
    cal->gy_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL2);
    cal->gz_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL3);
}
