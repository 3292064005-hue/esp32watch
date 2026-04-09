#include <Preferences.h>
#include <string.h>

extern "C" {
#include "persist.h"
#include "main.h"
#include "storage_packing.h"
}

namespace {
constexpr uint16_t BACKUP_MAGIC = 0xA55AU;
constexpr uint16_t PERSIST_CAL_VALID_FLAG = 0x1000U;
constexpr const char *kGameStatsNamespace = "watch_game";
constexpr const char *kGameStatsVersionKey = "version";
constexpr const char *kGameBreakoutKey = "breakout";
constexpr const char *kGameDinoKey = "dino";
constexpr const char *kGamePongKey = "pong";
constexpr const char *kGameSnakeKey = "snake";
constexpr const char *kGameTetrisKey = "tetris";
constexpr const char *kGameShooterKey = "shooter";

enum {
    BKP_DR_MAGIC = PLATFORM_RTC_BACKUP_REGISTER_0,
    BKP_DR_VERSION = PLATFORM_RTC_BACKUP_REGISTER_1,
    BKP_DR_CRC = PLATFORM_RTC_BACKUP_REGISTER_2,
    BKP_DR_SETTINGS0 = PLATFORM_RTC_BACKUP_REGISTER_3,
    BKP_DR_SETTINGS1 = PLATFORM_RTC_BACKUP_REGISTER_4,
    BKP_DR_SETTINGS2 = PLATFORM_RTC_BACKUP_REGISTER_5,
    BKP_DR_ALARM = PLATFORM_RTC_BACKUP_REGISTER_6,
    BKP_DR_ALARM1 = PLATFORM_RTC_BACKUP_REGISTER_7,
    BKP_DR_ALARM2 = PLATFORM_RTC_BACKUP_REGISTER_8,
    BKP_DR_ALARM_META = PLATFORM_RTC_BACKUP_REGISTER_9,
    BKP_DR_SENSOR_CAL0 = PLATFORM_RTC_BACKUP_REGISTER_10,
    BKP_DR_SENSOR_CAL1 = PLATFORM_RTC_BACKUP_REGISTER_11,
    BKP_DR_SENSOR_CAL2 = PLATFORM_RTC_BACKUP_REGISTER_12,
    BKP_DR_SENSOR_CAL3 = PLATFORM_RTC_BACKUP_REGISTER_13,
};

Preferences g_game_stats_prefs;

static bool persist_game_stats_open(bool read_only)
{
    return g_game_stats_prefs.begin(kGameStatsNamespace, read_only);
}

static uint16_t crc16_mix_u16(uint16_t crc, uint16_t value)
{
    crc ^= value;
    for (uint8_t bit = 0; bit < 16U; ++bit) {
        crc = (crc & 1U) ? (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
    }
    return crc;
}

static uint16_t calc_crc(uint16_t settings0, uint16_t settings1, uint16_t settings2,
                         uint16_t alarm0, uint16_t alarm1, uint16_t alarm2, uint16_t alarm_meta,
                         uint16_t cal0, uint16_t cal1, uint16_t cal2, uint16_t cal3)
{
    uint16_t crc = 0xFFFFU;
    crc = crc16_mix_u16(crc, settings0);
    crc = crc16_mix_u16(crc, settings1);
    crc = crc16_mix_u16(crc, settings2);
    crc = crc16_mix_u16(crc, alarm0);
    crc = crc16_mix_u16(crc, alarm1);
    crc = crc16_mix_u16(crc, alarm2);
    crc = crc16_mix_u16(crc, alarm_meta);
    crc = crc16_mix_u16(crc, cal0);
    crc = crc16_mix_u16(crc, cal1);
    crc = crc16_mix_u16(crc, cal2);
    crc = crc16_mix_u16(crc, cal3);
    return crc;
}

static void read_raw(uint16_t *settings0, uint16_t *settings1, uint16_t *settings2,
                     uint16_t *alarm0, uint16_t *alarm1, uint16_t *alarm2, uint16_t *alarm_meta,
                     uint16_t *cal0, uint16_t *cal1, uint16_t *cal2, uint16_t *cal3)
{
    *settings0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS0);
    *settings1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS1);
    *settings2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS2);
    *alarm0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM);
    *alarm1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM1);
    *alarm2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM2);
    *alarm_meta = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META);
    *cal0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL0);
    *cal1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL1);
    *cal2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL2);
    *cal3 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL3);
}

static void write_crc(void)
{
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_CRC, persist_get_calculated_crc());
}
} // namespace

extern "C" void persist_init(void)
{
    platform_rtc_backup_unlock();
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
    return calc_crc(settings0, settings1, settings2, alarm0, alarm1, alarm2, alarm_meta, cal0, cal1, cal2, cal3);
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

extern "C" void persist_save_settings(const SettingsState *settings)
{
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SETTINGS0, storage_pack_settings0(settings));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SETTINGS1, storage_pack_settings1(settings));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SETTINGS2, storage_pack_settings2(settings));
    write_crc();
}

extern "C" void persist_load_settings(SettingsState *settings)
{
    if (!persist_is_initialized()) {
        return;
    }
    storage_unpack_settings0(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS0), settings);
    settings->goal = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS1);
    if (settings->goal < 1000U || settings->goal > 30000U) {
        settings->goal = DEFAULT_STEP_GOAL;
    }
    if (settings->screen_timeout_idx > 2U) {
        settings->screen_timeout_idx = 0U;
    }
    if (settings->sensor_sensitivity > 2U) {
        settings->sensor_sensitivity = DEFAULT_SENSOR_SENSITIVITY;
    }
    if (settings->brightness > 3U) {
        settings->brightness = DEFAULT_BRIGHTNESS;
    }
    if (settings->watchface > 2U) {
        settings->watchface = DEFAULT_WATCHFACE;
    }
}

extern "C" void persist_save_alarms(const AlarmState *alarms, uint8_t count)
{
    AlarmState local[APP_MAX_ALARMS] = {0};
    uint16_t meta;
    for (uint8_t i = 0; i < APP_MAX_ALARMS && i < count; ++i) {
        local[i] = alarms[i];
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM, storage_pack_alarm_word(&local[0]));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM1, storage_pack_alarm_word(&local[1]));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM2, storage_pack_alarm_word(&local[2]));
    meta = storage_pack_alarm_meta(&local[0], &local[1], &local[2]);
    if ((platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & PERSIST_CAL_VALID_FLAG) != 0U) {
        meta |= PERSIST_CAL_VALID_FLAG;
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM_META, meta);
    write_crc();
}

extern "C" void persist_load_alarms(AlarmState *alarms, uint8_t count)
{
    uint16_t meta;
    if (alarms == NULL || count == 0U) {
        return;
    }
    for (uint8_t i = 0; i < count; ++i) {
        alarms[i].enabled = false;
        alarms[i].hour = DEFAULT_ALARM_HOUR;
        alarms[i].minute = DEFAULT_ALARM_MINUTE;
        alarms[i].repeat_mask = 0x7FU;
        alarms[i].label_id = i;
        alarms[i].ringing = false;
        alarms[i].snooze_until_epoch = 0U;
        alarms[i].last_trigger_day = 0U;
    }
    if (!persist_is_initialized()) {
        return;
    }
    meta = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META);
    if (count > 0U) {
        storage_unpack_alarm_word(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM), (uint8_t)(meta & 0x0FU), &alarms[0], 0U);
    }
    if (count > 1U) {
        storage_unpack_alarm_word(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM1), (uint8_t)((meta >> 4) & 0x0FU), &alarms[1], 1U);
    }
    if (count > 2U) {
        storage_unpack_alarm_word(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM2), (uint8_t)((meta >> 8) & 0x0FU), &alarms[2], 2U);
    }
}

extern "C" void persist_save_game_stats(const GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }
    if (!persist_game_stats_open(false)) {
        return;
    }
    g_game_stats_prefs.putUChar(kGameStatsVersionKey, APP_STORAGE_VERSION);
    g_game_stats_prefs.putULong(kGameBreakoutKey, stats->breakout_hi);
    g_game_stats_prefs.putULong(kGameDinoKey, stats->dino_hi);
    g_game_stats_prefs.putULong(kGamePongKey, stats->pong_hi);
    g_game_stats_prefs.putULong(kGameSnakeKey, stats->snake_hi);
    g_game_stats_prefs.putULong(kGameTetrisKey, stats->tetris_hi);
    g_game_stats_prefs.putULong(kGameShooterKey, stats->shooter_hi);
    g_game_stats_prefs.end();
}

extern "C" void persist_load_game_stats(GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    if (!persist_game_stats_open(true)) {
        return;
    }
    if (g_game_stats_prefs.getUChar(kGameStatsVersionKey, 0U) != APP_STORAGE_VERSION) {
        g_game_stats_prefs.end();
        return;
    }
    stats->breakout_hi = g_game_stats_prefs.getULong(kGameBreakoutKey, 0U);
    stats->dino_hi = g_game_stats_prefs.getULong(kGameDinoKey, 0U);
    stats->pong_hi = g_game_stats_prefs.getULong(kGamePongKey, 0U);
    stats->snake_hi = g_game_stats_prefs.getULong(kGameSnakeKey, 0U);
    stats->tetris_hi = g_game_stats_prefs.getULong(kGameTetrisKey, 0U);
    stats->shooter_hi = g_game_stats_prefs.getULong(kGameShooterKey, 0U);
    g_game_stats_prefs.end();
}

extern "C" void persist_save_sensor_calibration(const SensorCalibrationData *cal)
{
    uint16_t meta = (uint16_t)(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & 0x0FFFU);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    if (cal != NULL && cal->valid) {
        meta |= PERSIST_CAL_VALID_FLAG;
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM_META, meta);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL0, (uint16_t)((cal != NULL) ? cal->az_bias : 0));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL1, (uint16_t)((cal != NULL) ? cal->gx_bias : 0));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL2, (uint16_t)((cal != NULL) ? cal->gy_bias : 0));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL3, (uint16_t)((cal != NULL) ? cal->gz_bias : 0));
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
