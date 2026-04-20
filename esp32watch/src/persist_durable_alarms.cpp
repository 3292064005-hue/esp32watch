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

static bool persist_alarms_open(bool read_only)
{
    return persist_preferences_begin(g_alarms_prefs, PERSIST_PREFS_DOMAIN_APP_ALARMS, read_only);
}

void rtc_mirror_alarms(const AlarmState *alarms, uint8_t count)
{
    AlarmState local[APP_MAX_ALARMS] = {};
    uint16_t meta;
    for (uint8_t i = 0U; i < APP_MAX_ALARMS && i < count; ++i) {
        local[i] = alarms[i];
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM, storage_pack_alarm_word(&local[0]));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM1, storage_pack_alarm_word(&local[1]));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM2, storage_pack_alarm_word(&local[2]));
    meta = storage_pack_alarm_meta(&local[0], &local[1], &local[2]);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM_META, meta);
}

bool durable_store_alarms(const AlarmState *alarms, uint8_t count)
{
    if (!durable_app_state_enabled() || alarms == NULL) {
        return false;
    }
    DurableAlarmsRecord record = persist_codec_build_alarms_record(alarms, count, PERSIST_ALARMS_RECORD_MAGIC, APP_STORAGE_VERSION);
    if (!persist_alarms_open(false)) {
        return false;
    }
    const bool ok = g_alarms_prefs.putBytes(PERSIST_ALARMS_RECORD_KEY, &record, sizeof(record)) == sizeof(record);
    g_alarms_prefs.end();
    rtc_mirror_alarms(alarms, count);
    return ok;
}

bool durable_load_alarms(AlarmState *alarms, uint8_t count)
{
    DurableAlarmsRecord record = {};
    if (!durable_app_state_enabled() || alarms == NULL || count == 0U) {
        return false;
    }
    if (!persist_alarms_open(true)) {
        return false;
    }
    const bool loaded = g_alarms_prefs.getBytes(PERSIST_ALARMS_RECORD_KEY, &record, sizeof(record)) == sizeof(record) &&
                        persist_codec_alarms_record_valid(&record, PERSIST_ALARMS_RECORD_MAGIC, APP_STORAGE_VERSION);
    g_alarms_prefs.end();
    if (!loaded) {
        return false;
    }
    memset(alarms, 0, sizeof(*alarms) * count);
    if (count > 0U) {
        storage_unpack_alarm_word(record.alarm0, (uint8_t)(record.alarm_meta & 0x0FU), &alarms[0], 0U);
    }
    if (count > 1U) {
        storage_unpack_alarm_word(record.alarm1, (uint8_t)((record.alarm_meta >> 4) & 0x0FU), &alarms[1], 1U);
    }
    if (count > 2U) {
        storage_unpack_alarm_word(record.alarm2, (uint8_t)((record.alarm_meta >> 8) & 0x0FU), &alarms[2], 2U);
    }
    rtc_mirror_alarms(alarms, count);
    return true;
}

extern "C" void persist_save_alarms(const AlarmState *alarms, uint8_t count)
{
    if (alarms == NULL || count == 0U) {
        return;
    }
    if (!durable_store_alarms(alarms, count)) {
        rtc_mirror_alarms(alarms, count);
    }
}

extern "C" void persist_load_alarms(AlarmState *alarms, uint8_t count)
{
    if (alarms == NULL || count == 0U) {
        return;
    }
    if (!durable_load_alarms(alarms, count)) {
        memset(alarms, 0, sizeof(*alarms) * count);
        if (count > 0U) {
            storage_unpack_alarm_word((uint16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM),
                                      (uint8_t)(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & 0x0FU),
                                      &alarms[0],
                                      0U);
        }
        if (count > 1U) {
            storage_unpack_alarm_word((uint16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM1),
                                      (uint8_t)((platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) >> 4) & 0x0FU),
                                      &alarms[1],
                                      1U);
        }
        if (count > 2U) {
            storage_unpack_alarm_word((uint16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM2),
                                      (uint8_t)((platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) >> 8) & 0x0FU),
                                      &alarms[2],
                                      2U);
        }
    }
}
