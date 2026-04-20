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

Preferences g_game_stats_prefs;
Preferences g_settings_prefs;
Preferences g_alarms_prefs;
Preferences g_calibration_prefs;

extern "C" void persist_init(void)
{
    platform_rtc_backup_unlock();
    durable_migrate_or_mirror();
}

extern "C" bool persist_app_state_durable_ready(void)
{
    return durable_app_state_backend_ready_internal();
}

extern "C" const char *persist_app_state_backend_name(void)
{
    return persist_app_state_durable_ready() ? "NVS_APP_STATE" : "RTC_RESET_DOMAIN";
}

extern "C" uint8_t persist_get_version(void)
{
    return (uint8_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_VERSION);
}

extern "C" uint16_t persist_get_stored_crc(void)
{
    return (uint16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_CRC);
}

extern "C" uint16_t persist_get_calculated_crc(void)
{
    uint16_t settings0, settings1, settings2, alarm0, alarm1, alarm2, alarm_meta, cal0, cal1, cal2, cal3;
    read_raw(&settings0, &settings1, &settings2, &alarm0, &alarm1, &alarm2, &alarm_meta, &cal0, &cal1, &cal2, &cal3);
    return persist_codec_calc_crc_raw(settings0, settings1, settings2, alarm0, alarm1, alarm2, alarm_meta, cal0, cal1, cal2, cal3);
}

extern "C" bool persist_is_initialized(void)
{
    if (platform_rtc_backup_read(&platform_rtc_main, BKP_DR_MAGIC) != BACKUP_MAGIC) {
        return false;
    }
    if (persist_get_version() != APP_STORAGE_VERSION) {
        return false;
    }
    return persist_get_stored_crc() == persist_get_calculated_crc();
}

